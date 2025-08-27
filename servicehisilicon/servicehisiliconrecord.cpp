#include "servicehisiliconrecord.h"

#include <lib/base/eerror.h>      // eDebug
#include <lib/service/iservice.h>
#include <lib/dvb/idvb.h>

#include <string>
#include <cstring>
#include <cctype>

// FFmpeg (C-API)
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
}

// ---- kleine Hilfsfunktionen -------------------------------------------------

static bool endswith_ci(const std::string &s, const char *suffix)
{
    const size_t n = s.size();
    const size_t m = std::strlen(suffix);
    if (m > n) return false;
    for (size_t i = 0; i < m; ++i)
    {
        char a = std::tolower(static_cast<unsigned char>(s[n - m + i]));
        char b = std::tolower(static_cast<unsigned char>(suffix[i]));
        if (a != b) return false;
    }
    return true;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

// sehr einfache URL-Decodierung (nur %HH), ausreichend für http%3a//...
static std::string url_decode_percent(const std::string &in)
{
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i)
    {
        if (in[i] == '%' && i + 2 < in.size())
        {
            int h1 = hexval(in[i+1]);
            int h2 = hexval(in[i+2]);
            if (h1 >= 0 && h2 >= 0)
            {
                out.push_back(static_cast<char>((h1 << 4) | h2));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

// ---- Klassen-Implementation -------------------------------------------------

DEFINE_REF(eServiceHisiliconRecord);

eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
    : m_state(stateIdle), m_error(0), m_ref(ref), m_simulate(false)
{
}

eServiceHisiliconRecord::~eServiceHisiliconRecord()
{
    stop();
}

RESULT eServiceHisiliconRecord::connectEvent(
    const sigc::slot<void(iRecordableService*, int)> &event,
    ePtr<eConnection> &connection)
{
    connection = new eConnection(this, m_event.connect(event));
    return 0;
}

RESULT eServiceHisiliconRecord::prepare(
    const char *filename, time_t /*begTime*/, time_t /*endTime*/, int /*eit_event_id*/,
    const char * /*name*/, const char * /*descr*/, const char * /*tags*/,
    bool /*descramble*/, bool /*recordecm*/, int /*packetsize*/)
{
    m_filename = filename ? filename : "/media/hdd/default.stream";
    m_state = statePrepared;
    m_error = 0;
    return 0;
}

RESULT eServiceHisiliconRecord::prepareStreaming(bool /*descramble*/, bool /*includeecm*/)
{
    m_state = statePrepared;
    m_error = 0;
    return 0;
}

RESULT eServiceHisiliconRecord::start(bool simulate)
{
    m_simulate = simulate;

    if (m_state != statePrepared)
        return -1;

    int result = doRecord();  // aktuell: nur Header/Trailer → schneller Test
    if (result == 0)
    {
        m_event(this, evRecordStarted);
        m_state = stateRecording;
        // Da wir noch keinen Loop haben, stoppen wir sofort wieder „sauber“,
        // damit der Timer / Enigma2 nicht hängen bleibt.
        stop();
    }
    return result;
}

int eServiceHisiliconRecord::doRecord()
{
    // 1) Eingabe-URL aus dem ServiceReference ermitteln
    //    Für 4097:...:http%3a//... steckt die URL gewöhnlich in ref.path
    std::string inputUrl = url_decode_percent(m_ref.path);
    if (inputUrl.empty())
    {
        eDebug("[HisiliconRecord] input URL leer (m_ref.path).");
        return -1;
    }

    // 2) FFmpeg initialisieren
    avformat_network_init();

    AVFormatContext *inCtx  = nullptr;
    AVFormatContext *outCtx = nullptr;
    int ret = 0;

    // 2a) Input öffnen (automatische Erkennung: .ts, .m3u8, ...)
    if ((ret = avformat_open_input(&inCtx, inputUrl.c_str(), NULL, NULL)) < 0)
    {
        eDebug("[HisiliconRecord] avformat_open_input failed for '%s' (ret=%d)", inputUrl.c_str(), ret);
        goto fail;
    }
    if ((ret = avformat_find_stream_info(inCtx, NULL)) < 0)
    {
        eDebug("[HisiliconRecord] avformat_find_stream_info failed (ret=%d)", ret);
        goto fail;
    }

    // 3) Output-Kontext: .stream explizit als MPEG-TS behandeln
    if (endswith_ci(m_filename, ".stream"))
        ret = avformat_alloc_output_context2(&outCtx, NULL, "mpegts", m_filename.c_str());
    else
        ret = avformat_alloc_output_context2(&outCtx, NULL, NULL, m_filename.c_str());

    if (ret < 0 || !outCtx)
    {
        eDebug("[HisiliconRecord] avformat_alloc_output_context2 failed (ret=%d), file='%s'", ret, m_filename.c_str());
        goto fail;
    }

    // 4) (Vorbereitend) – Ausgabe-Datei öffnen
    if (!(outCtx->oformat->flags & AVFMT_NOFILE))
    {
        if ((ret = avio_open(&outCtx->pb, m_filename.c_str(), AVIO_FLAG_WRITE)) < 0)
        {
            eDebug("[HisiliconRecord] avio_open failed for '%s' (ret=%d)", m_filename.c_str(), ret);
            goto fail;
        }
    }

    // 5) Aktuell: wir schreiben nur einen leeren Container (Header/Trailer),
    //    um zu prüfen, dass das Format richtig gewählt wurde (v.a. bei .stream).
    if ((ret = avformat_write_header(outCtx, NULL)) < 0)
    {
        eDebug("[HisiliconRecord] avformat_write_header failed (ret=%d)", ret);
        goto fail;
    }

    // Noch KEIN Kopier-Loop – das fügen wir im nächsten Schritt ein
    if ((ret = av_write_trailer(outCtx)) < 0)
    {
        eDebug("[HisiliconRecord] av_write_trailer failed (ret=%d)", ret);
        goto fail;
    }

    // Erfolg
    avformat_close_input(&inCtx);
    if (outCtx)
    {
        if (!(outCtx->oformat->flags & AVFMT_NOFILE) && outCtx->pb)
            avio_closep(&outCtx->pb);
        avformat_free_context(outCtx);
    }
    eDebug("[HisiliconRecord] wrote empty container OK: '%s'", m_filename.c_str());
    return 0;

fail:
    if (inCtx)
        avformat_close_input(&inCtx);
    if (outCtx)
    {
        if (!(outCtx->oformat->flags & AVFMT_NOFILE) && outCtx->pb)
            avio_closep(&outCtx->pb);
        avformat_free_context(outCtx);
    }
    m_error = 1;
    return -1;
}

RESULT eServiceHisiliconRecord::stop()
{
    if (m_state == stateRecording)
    {
        m_state = stateIdle;
        m_event(this, evRecordStopped);
    }
    return 0;
}

RESULT eServiceHisiliconRecord::stream(ePtr<iStreamableService> &ptr)
{
    ptr = 0;
    return -1;  // optional implementierbar
}

RESULT eServiceHisiliconRecord::frontendInfo(ePtr<iFrontendInformation> &ptr)
{
    ptr = 0;
    return -1;  // optional implementierbar
}

RESULT eServiceHisiliconRecord::subServices(ePtr<iSubserviceList> &ptr)
{
    ptr = 0;
    return -1;
}

