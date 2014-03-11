#include <engine.hh>
#include <configuration.hh>
#include <output-handler.hh>
#include <lineid.hh>
#include <utils.hh>
#include <zlib.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>

#include <list>
#include <unordered_map>

#include "../swap-endian.hh"

using namespace kcov;

class KernelEngine : public IEngine
{
public:
	KernelEngine() :
		m_control(NULL),
		m_show(NULL)
	{
		IEngineFactory::getInstance().registerEngine(*this);
	}

	// From IEngine
	int registerBreakpoint(unsigned long addr)
	{
		std::string s = fmt("%s0x%llx\n", m_module.c_str(), (unsigned long long)addr);

		printf("%s", s.c_str());
		panic_if (!m_control,
				"Control file not open???");

		fprintf(m_control, "%s", s.c_str());
		fflush(m_control);

		return 0;
	}

	void setupAllBreakpoints()
	{
	}

	bool clearBreakpoint(int id)
	{
		return true;
	}

	bool start(IEventListener &listener, const std::string &executable)
	{
		std::string path = IConfiguration::getInstance().getKernelCoveragePath();
		auto control = path + "/control";
		auto show = path + "/show";

		// Open kprobe-coverage files
		m_control = fopen(control.c_str(), "w");
		m_show = fopen(show.c_str(), "r");

		if (!m_control || !m_show) {
			error("Can't open kprobe-coverage files. Is the kprobe-coverage module loaded?");

			kill();
			return false;
		}

		return true;
	}

	bool continueExecution()
	{
		char buf[256]; // More than enough for one line
		char *p;

		p = fgets(buf, sizeof(buf) - 1, m_show);
		if (!p)
			return false;

		buf[sizeof(buf) - 1] = '\0';
		printf("XXX: Got %s\n", buf);

		return true;
	}

	bool childrenLeft()
	{
		return true;
	}

	void kill()
	{
		if (m_control) {
			fprintf(m_control, "clear\n");
			fclose(m_control);
		}
		if (m_show)
			fclose(m_show);

		m_control = NULL;
		m_show = NULL;
	}

	unsigned int matchFile(const std::string &filename, uint8_t *data, size_t dataSize)
	{
		// FIXME! Only works for kernel modules
		if (filename.find(".ko") == std::string::npos)
			return match_none;

		m_module = filename;
		auto slashPos = m_module.rfind("/");
		if (slashPos == std::string::npos)
			slashPos = 0;
		else
			slashPos++; // Skip the actual slash

		// Remove path before and .ko after the name
		m_module = m_module.substr(slashPos, m_module.size() - slashPos - 3) + ":";

		return match_perfect;
	}

private:
	FILE *m_control;
	FILE *m_show;

	std::string m_module;
};

static KernelEngine g_instance;