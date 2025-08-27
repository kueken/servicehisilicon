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

DEFINE_REF(eServiceHisiliconRecord);

eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
    : m_state(stateIdle), m_error(0), m_ref(ref), m_simulate(false), m_ffmpegPid(-1)
{
    eDebug("[eServiceHisiliconRecord] construct for %s", m_ref.path.c_str());
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
    eDebug("[eServiceHisiliconRecord] prepared, filename=%s", m_filename.c_str());
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
        eDebug("[eServiceHisiliconRecord] recording started with ffmpeg PID=%d", m_ffmpegPid);
    }
    return result;
}

int eServiceHisiliconRecord::doRecord()
{
    if (m_filename.empty())
        return -1;

    if (m_state == stateRecording)
        return -1;

    pid_t pid = fork();
    if (pid == 0)
    {
        // Kindprozess: ffmpeg starten
        execlp("ffmpeg", "ffmpeg",
               "-y",                       // überschreiben falls Datei existiert
               "-i", m_ref.path.c_str(),   // Input-URL aus ServiceReference
               "-c", "copy",               // ohne Transcoding
               m_filename.c_str(),         // Ziel-Datei
               (char*)nullptr);

        // Falls execlp fehlschlägt:
        _exit(1);
    }
    else if (pid > 0)
    {
        m_ffmpegPid = pid;
        return 0;
    }
    else
    {
        eDebug("[eServiceHisiliconRecord] fork() failed!");
        return -1;
    }
}

RESULT eServiceHisiliconRecord::stop()
{
    if (m_state == stateRecording && m_ffmpegPid > 0)
    {
        eDebug("[eServiceHisiliconRecord] stopping ffmpeg PID=%d", m_ffmpegPid);
        kill(m_ffmpegPid, SIGTERM);
        waitpid(m_ffmpegPid, nullptr, 0);
        m_ffmpegPid = -1;
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

