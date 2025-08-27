#ifndef __servicehisiliconrecord_h
#define __servicehisiliconrecord_h

#include <lib/service/servicerecord.h>
#include <lib/dvb/idvb.h>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class eServiceHisiliconRecord: public iRecordableService, public Object
{
	DECLARE_REF(eServiceHisiliconRecord);

public:
	eServiceHisiliconRecord(const eServiceReference &ref);
	~eServiceHisiliconRecord();

	// iRecordableService Interface
	RESULT start();
	RESULT stop();

	void setFilename(const std::string &filename) { m_filename = filename; }

private:
	eServiceReference m_ref;
	std::string m_filename;

	// Status
	bool m_running;

	// FFmpeg Kontexte (nur bei Streams benutzt)
	AVFormatContext *fmt_ctx;
	AVFormatContext *out_ctx;
};

#endif

