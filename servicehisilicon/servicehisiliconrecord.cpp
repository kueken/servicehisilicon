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
#include <lib/dvb/decoder.h>


eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
    : m_state(stateIdle), m_error(0), m_ref(ref), m_simulate(false)
{
}

eServiceHisiliconRecord::~eServiceHisiliconRecord()
{
    stop();
}

RESULT eServiceHisiliconRecord::connectEvent(const sigc::slot<void(iRecordableService*, int)> &event, ePtr<eConnection> &connection)
{
    connection = new eConnection(this, m_event.connect(event));
    return 0;
}

RESULT eServiceHisiliconRecord::prepare(const char *filename, time_t begTime, time_t endTime, int eit_event_id,
                                        const char *name, const char *descr, const char *tags, bool descramble,
                                        bool recordecm, int packetsize)
{
    m_filename = filename ? filename : "/media/hdd/default.ts";
    m_state = statePrepared;
    m_error = 0;
    return 0;
}

RESULT eServiceHisiliconRecord::prepareStreaming(bool descramble, bool includeecm)
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

    int result = doRecord();
    if (result == 0)
    {
        m_event(this, evRecordStarted);
        m_state = stateRecording;
    }
    return result;
}

int eServiceHisiliconRecord::doRecord()
{
    // Hier könnte man den Start der Hardware-Demux-Aufnahme triggern
    // Momentan nur Stub
    return 0;
}

RESULT eServiceHisiliconRecord::stop()
{
    if (m_state == stateRecording)
    {
        // Aufnahme stoppen – bei echter Implementierung Hardware-Demux beenden
        m_state = stateIdle;
        m_event(this, evRecordStopped);
    }
    return 0;
}

RESULT eServiceHisiliconRecord::stream(ePtr<iStreamableService> &ptr)
{
    return -1;  // optional implementierbar
}

RESULT eServiceHisiliconRecord::frontendInfo(ePtr<iFrontendInformation> &ptr)
{
    return -1;  // optional implementierbar
}

RESULT eServiceHisiliconRecord::subServices(ePtr<iSubserviceList> &ptr)
{
    return -1;
}

// sorgt dafür, dass der Compiler die vtable wirklich erzeugt
template class ePtr<eServiceHisiliconRecord>;

