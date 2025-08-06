#ifndef __servicehisiliconrecord_h
#define __servicehisiliconrecord_h

#include <lib/service/iservice.h>
#include <lib/dvb/idvb.h>
#include "servicehisilicon.h"

class eServiceHisiliconRecord:
	public iRecordableService,
	public sigc::trackable
{
	DECLARE_REF(eServiceHisiliconRecord);
public:
	RESULT connectEvent(const sigc::slot<void(iRecordableService*,int)> &event, ePtr<eConnection> &connection);
	RESULT prepare(const char *filename, time_t begTime, time_t endTime, int eit_event_id, const char *name, const char *descr, const char *tags, bool descramble, bool recordecm, int packetsize);
	RESULT prepareStreaming(bool descramble, bool includeecm);
	RESULT start(bool simulate=false);
	RESULT stop();
	RESULT stream(ePtr<iStreamableService> &ptr);
	RESULT getError(int &error) { error = m_error; return 0; }
	RESULT frontendInfo(ePtr<iFrontendInformation> &ptr) { ptr = 0; return -1; }
	RESULT subServices(ePtr<iSubserviceList> &ptr) { ptr = 0; return -1; }
	RESULT getFilenameExtension(std::string &ext) { ext = ".ts"; return 0; }

private:
	enum { stateIdle, statePrepared, stateRecording };

	int m_state;
	int m_error;
	bool m_simulate;
	int m_dvr_fd;

	std::string m_filename;
	eServiceReference m_ref;
	ePtr<eConnection> m_con_record_event;
	sigc::signal<void(iRecordableService*,int)> m_event;

	eServiceHisiliconRecord(const eServiceReference &ref);
	~eServiceHisiliconRecord();
	int doPrepare();
	int doRecord();
};

#endif

