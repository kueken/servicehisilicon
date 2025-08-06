#include "servicehisiliconrecord.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

eServiceHisiliconRecord::eServiceHisiliconRecord(const eServiceReference &ref)
	: m_state(stateIdle), m_error(0), m_simulate(false), m_dvr_fd(-1), m_ref(ref)
{
}

eServiceHisiliconRecord::~eServiceHisiliconRecord()
{
	stop();
}

RESULT eServiceHisiliconRecord::connectEvent(const sigc::slot<void(iRecordableService*,int)> &event, ePtr<eConnection> &connection)
{
	connection = new eConnection((sigc::signal<void(iRecordableService*,int)>*)&m_event, event);
	return 0;
}

RESULT eServiceHisiliconRecord::prepare(const char *filename, time_t begTime, time_t endTime, int eit_event_id, const char *name, const char *descr, const char *tags, bool descramble, bool recordecm, int packetsize)
{
	if (m_state != stateIdle)
		return -1;

	m_filename = filename;
	return doPrepare();
}

RESULT eServiceHisiliconRecord::prepareStreaming(bool descramble, bool includeecm)
{
	// Streaming nicht implementiert
	return -1;
}

int eServiceHisiliconRecord::doPrepare()
{
	m_dvr_fd = open("/dev/dvb/adapter0/dvr0", O_RDONLY);
	if (m_dvr_fd < 0)
	{
		m_error = errno;
		return -1;
	}
	m_state = statePrepared;
	return 0;
}

RESULT eServiceHisiliconRecord::start(bool simulate)
{
	if (m_state != statePrepared)
		return -1;

	m_simulate = simulate;
	if (m_simulate)
	{
		m_state = stateRecording;
		m_event(this, evRecordStarted);
		return 0;
	}

	return doRecord();
}

int eServiceHisiliconRecord::doRecord()
{
	int fd = open(m_filename.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd < 0)
	{
		m_error = errno;
		return -1;
	}

	m_state = stateRecording;
	m_event(this, evRecordStarted);

	char buffer[188 * 256]; // TS packets
	ssize_t r;

	while ((r = read(m_dvr_fd, buffer, sizeof(buffer))) > 0)
	{
		if (write(fd, buffer, r) != r)
			break;
	}

	close(fd);
	m_event(this, evRecordStopped);
	m_state = stateIdle;
	return 0;
}

RESULT eServiceHisiliconRecord::stop()
{
	if (m_state != stateRecording && m_state != statePrepared)
		return -1;

	if (m_dvr_fd >= 0)
	{
		close(m_dvr_fd);
		m_dvr_fd = -1;
	}

	m_state = stateIdle;
	m_event(this, evRecordStopped);
	return 0;
}

RESULT eServiceHisiliconRecord::stream(ePtr<iStreamableService> &ptr)
{
	ptr = nullptr;
	return -1;
}

