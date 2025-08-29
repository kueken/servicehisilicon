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
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
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

// ----- Version Compatibility Macros -----
#if LIBAVFORMAT_VERSION_MAJOR < 58
#define MY_AV_REGISTER_ALL()   av_register_all(); avcodec_register_all();
#define MY_AV_OPEN_INPUT(ctx, file)  av_open_input_file(&(ctx), file, NULL, 0, NULL)
#define MY_AV_CLOSE_INPUT(ctx) av_close_input_file(ctx)
#define MY_AV_FREE_PACKET(pkt) av_free_packet(pkt)
#else
#define MY_AV_REGISTER_ALL()
#define MY_AV_OPEN_INPUT(ctx, file)  avformat_open_input(&(ctx), file, NULL, NULL)
#define MY_AV_CLOSE_INPUT(ctx) avformat_close_input(&(ctx))
#define MY_AV_FREE_PACKET(pkt) av_packet_unref(&(pkt))
#endif

// ---- Klassen-Implementation -------------------------------------------------

DEFINE_REF(eServiceHisiliconRecord);

eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
    : m_ref(ref), m_outctx(nullptr)
{
    MY_AV_REGISTER_ALL();
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

RESULT eServiceHisiliconRecord::start(const char *filename)
{
    int ret;

    // Output format context
    if (endswith(filename, ".stream"))
        ret = avformat_alloc_output_context2(&m_outctx, NULL, "mpegts", filename);
    else
        ret = avformat_alloc_output_context2(&m_outctx, NULL, NULL, filename);

    if (!m_outctx || ret < 0) {
        eDebug("[eServiceHisiliconRecord] failed to alloc output ctx");
        return -1;
    }

    // open file
    if (!(m_outctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&m_outctx->pb, filename, AVIO_FLAG_WRITE) < 0) {
            eDebug("[eServiceHisiliconRecord] could not open output file %s", filename);
            return -1;
        }
    }

    // write header
    if (avformat_write_header(m_outctx, NULL) < 0) {
        eDebug("[eServiceHisiliconRecord] failed to write header");
        return -1;
    }

    eDebug("[eServiceHisiliconRecord] recording started -> %s", filename);
    return 0;
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
    if (m_outctx) {
        av_write_trailer(m_outctx);

        if (!(m_outctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_outctx->pb);
        }

        avformat_free_context(m_outctx);
        m_outctx = nullptr;

        eDebug("[eServiceHisiliconRecord] recording stopped");
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

