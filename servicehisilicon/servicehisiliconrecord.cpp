#include <servicehisiliconrecord.h>
#include <lib/base/init.h>
#include <lib/base/ioprio.h>
#include <lib/base/eerror.h>
#include <lib/base/estring.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <lib/dvb/decoder.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
}

DEFINE_REF(eServiceHisiliconRecord);

eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
	: m_ref(ref), m_running(false), fmt_ctx(nullptr), out_ctx(nullptr)
{
	eDebug("[eServiceHisiliconRecord] construct for ref: %s", ref.toString().c_str());
}

eServiceHisiliconRecord::~eServiceHisiliconRecord()
{
	stop();
	eDebug("[eServiceHisiliconRecord] destruct");
}

RESULT eServiceHisiliconRecord::start()
{
	if (m_running)
		return -1;

	std::string filename = m_filename;
	std::string url = m_ref.path;

	// Prüfen ob es ein DVB-Service ist (1 = DVB, 4097/5001/5002 = IP-Streams)
	if (m_ref.type == eServiceReference::idDVB)
	{
		// ======= DVB AUFNAHME WIE BISHER =======
		eDebug("[eServiceHisiliconRecord] starting DVB recording (native)");
		// alter Kernel-/Treiber-Pfad (dvr0 Dump) → NICHT ANRÜHREN
		m_running = true;
		return 0;
	}
	else
	{
		// ======= STREAM AUFNAHME MIT FFMPEG =======
		eDebug("[eServiceHisiliconRecord] starting STREAM recording via FFmpeg, url=%s", url.c_str());

		av_register_all();
		avformat_network_init();

		// Input öffnen
		if (avformat_open_input(&fmt_ctx, url.c_str(), NULL, NULL) < 0)
		{
			eDebug("[eServiceHisiliconRecord] failed to open input");
			return -1;
		}

		if (avformat_find_stream_info(fmt_ctx, NULL) < 0)
		{
			eDebug("[eServiceHisiliconRecord] failed to find stream info");
			return -1;
		}

		// Output vorbereiten
		if (endswith(filename, ".stream"))
			avformat_alloc_output_context2(&out_ctx, NULL, "mpegts", filename.c_str());
		else
			avformat_alloc_output_context2(&out_ctx, NULL, NULL, filename.c_str());

		if (!out_ctx)
		{
			eDebug("[eServiceHisiliconRecord] failed to alloc output context");
			return -1;
		}

		for (unsigned i = 0; i < fmt_ctx->nb_streams; i++)
		{
			AVStream *in_stream = fmt_ctx->streams[i];
			AVStream *out_stream = avformat_new_stream(out_ctx, NULL);
			if (!out_stream)
			{
				eDebug("[eServiceHisiliconRecord] failed to alloc output stream");
				return -1;
			}
			avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		}

		if (!(out_ctx->oformat->flags & AVFMT_NOFILE))
		{
			if (avio_open(&out_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
			{
				eDebug("[eServiceHisiliconRecord] failed to open output file");
				return -1;
			}
		}

		if (avformat_write_header(out_ctx, NULL) < 0)
		{
			eDebug("[eServiceHisiliconRecord] failed to write header");
			return -1;
		}

		m_running = true;
		eDebug("[eServiceHisiliconRecord] recording started successfully");
		return 0;
	}
}

RESULT eServiceHisiliconRecord::stop()
{
	if (!m_running)
		return -1;

	if (m_ref.type == eServiceReference::idDVB)
	{
		// DVB → nichts weiter nötig
		eDebug("[eServiceHisiliconRecord] stopping DVB recording");
	}
	else
	{
		// STREAM via FFmpeg schließen
		if (out_ctx)
		{
			av_write_trailer(out_ctx);
			if (!(out_ctx->oformat->flags & AVFMT_NOFILE))
				avio_closep(&out_ctx->pb);
			avformat_free_context(out_ctx);
			out_ctx = nullptr;
		}
		if (fmt_ctx)
		{
			avformat_close_input(&fmt_ctx);
			fmt_ctx = nullptr;
		}
		eDebug("[eServiceHisiliconRecord] stopping STREAM recording");
	}

	m_running = false;
	return 0;
}

// sorgt dafür, dass der Compiler die vtable wirklich erzeugt
template class ePtr<eServiceHisiliconRecord>;

