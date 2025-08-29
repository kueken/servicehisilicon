#ifndef __servicehisiliconrecord_h
#define __servicehisiliconrecord_h

#include <lib/service/iservice.h>
#include <lib/dvb/idvb.h>
#include <servicehisilicon.h>
#include <lib/base/object.h>
#include <lib/base/ebase.h>

class eServiceHisiliconRecord :
    public iRecordableService,
    public sigc::trackable
{
    DECLARE_REF(eServiceHisiliconRecord);

public:
    eServiceHisiliconRecord(const eServiceReference &ref);
    virtual ~eServiceHisiliconRecord();  // <-- vtable sicherstellen

    RESULT connectEvent(const sigc::slot<void(iRecordableService*, int)> &event, ePtr<eConnection> &connection);
    RESULT prepare(const char *filename, time_t begTime, time_t endTime, int eit_event_id,
                   const char *name, const char *descr, const char *tags, bool descramble,
                   bool recordecm, int packetsize);
    RESULT prepareStreaming(bool descramble, bool includeecm);
    RESULT start(const char *filename);
    RESULT stop();
    RESULT stream(ePtr<iStreamableService> &ptr);
    RESULT getError(int &error) { error = m_error; return 0; };
    RESULT frontendInfo(ePtr<iFrontendInformation> &ptr);
    RESULT subServices(ePtr<iSubserviceList> &ptr);
    RESULT getFilenameExtension(std::string &ext) { ext = ".stream"; return 0; };

private:
    pid_t m_ffmpegPid = -1;

private:
    enum {
        stateIdle,
        statePrepared,
        stateRecording
    };

    enum {
        evRecordStarted = 1,
        evRecordStopped = 2,
        evRecordWriteError = 3
    };

    int m_state;
    int m_error;
    std::string m_filename;
    int doRecord();
    eServiceReference m_ref;

    sigc::signal<void(iRecordableService*, int)> m_event;
};

#endif

