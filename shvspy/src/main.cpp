#include "mainwindow.h"
#include "theapp.h"
#include "appversion.h"
#include "appclioptions.h"

#include <shv/chainpack/rpcmessage.h>
#include <shv/core/utils.h>
#include <shv/coreqt/log.h>
#include <shv/coreqt/utils.h>

#include <QTextStream>
#include <QTranslator>
#include <QDateTime>
#include <QSettings>
#include <QFile>

#include <iostream>

//#define PROFILE_SOMETHING
#ifdef PROFILE_SOMETHING
#include <shv/core/utils/shvjournalfilereader.h>
#include <shv/core/utils/shvjournalfilewriter.h>
#include <shv/core/utils/shvjournalentry.h>

#include <chrono>

void profile_something()
{
	using namespace std::string_literals;
	using namespace std;
	using namespace std::chrono;
	static int n = 0;
	auto file_name = "/tmp/dirty.log"s;
	int64_t write_time = 0;
	int64_t read_time = 0;
	for(int i = 0; i < 1000; i++) {
		{
			auto start = high_resolution_clock::now();
			{
				auto writer = shv::core::utils::ShvJournalFileWriter(file_name);
				auto path_without_prefix = "a/b/c"s;
				auto data_change = shv::chainpack::DataChange::fromRpcValue(++n);

				auto entry = shv::core::utils::ShvJournalEntry(path_without_prefix, data_change.value()
														, shv::core::utils::ShvJournalEntry::DOMAIN_VAL_CHANGE
														, shv::core::utils::ShvJournalEntry::NO_SHORT_TIME
														, shv::core::utils::ShvJournalEntry::NO_VALUE_FLAGS
														, data_change.epochMSec());
				writer.append(entry);
			}
			auto stop = high_resolution_clock::now();
			write_time += duration_cast<microseconds>(stop - start).count();
		}
		{
			auto start = high_resolution_clock::now();
			{
				shv::core::utils::ShvJournalFileReader reader(file_name);
				reader.next(); // There must be at least one entry, because I've just written it.
			}
			auto stop = high_resolution_clock::now();
			read_time += duration_cast<microseconds>(stop - start).count();
		}
	}
	shvInfo() << "write time:" << write_time << "usec";
	shvInfo() << "read time:" << read_time << "usec";
}
#endif

int main(int argc, char *argv[])
{
	// call something from shv::coreqt to avoid linker error:
	// error while loading shared libraries: libshvcoreqt.so.1: cannot open shared object file: No such file or directory
	shv::coreqt::Utils::isDefaultQVariantValue(QVariant());

	QCoreApplication::setOrganizationName("Elektroline");
	QCoreApplication::setOrganizationDomain("elektroline.cz");
	QCoreApplication::setApplicationName("shvspy");
	QCoreApplication::setApplicationVersion(APP_VERSION);
#ifdef Q_OS_WASM
	NecroLog::setColorizedOutputMode(NecroLog::ColorizedOutputMode::No);
#endif
	std::vector<std::string> shv_args = NecroLog::setCLIOptions(argc, argv);

	int ret = 0;
#ifdef PROFILE_SOMETHING
	profile_something();
	return ret;
#endif
	AppCliOptions cli_opts;
	cli_opts.parse(shv_args);
	if(cli_opts.isParseError()) {
		for(const std::string &err : cli_opts.parseErrors())
			shvError() << err;
		return EXIT_FAILURE;
	}
	if(cli_opts.isAppBreak()) {
		if(cli_opts.isHelp()) {
			cli_opts.printHelp(std::cout);
		}
		return EXIT_SUCCESS;
	}
	for(const std::string &s : cli_opts.unusedArguments()) {
		shvWarning() << "Undefined argument:" << s;
	}

	if(cli_opts.isRawRpcMessageLog()) {
		auto s = NecroLog::topicsLogThresholds();
		if(!s.empty())
			s += ",";
		s += "RpcMsg";
		NecroLog::setTopicsLogThresholds(s);
	}

	shv::chainpack::RpcMessage::registerMetaTypes();

	shvInfo() << "======================================================================================";
	shvInfo() << "Starting" << QCoreApplication::applicationName()
			  << "ver." << QCoreApplication::applicationVersion()
			  << "PID:" << QCoreApplication::applicationPid();
#ifdef GIT_COMMIT
	shvInfo() << "GIT commit:" << SHV_EXPAND_AND_QUOTE(GIT_COMMIT);
#endif
	shvInfo() << QDateTime::currentDateTime().toString(Qt::ISODate).toStdString() << "UTC:" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
	shvInfo() << "======================================================================================";
	shvInfo() << "Log tresholds:" << NecroLog::thresholdsLogInfo();

	if(cli_opts.configDir_isset()) {
		auto config_dir = QString::fromStdString(cli_opts.configDir());
		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config_dir);
		shvInfo() << "Config dir:" << config_dir;
	}

	shvInfo() << "--------------------------------------------------------------------------------------";

	TheApp a(argc, argv, &cli_opts);
	MainWindow w;
	w.show();

	shvInfo() << "starting main thread event loop";
	ret = TheApp::exec();
	shvInfo() << "main event loop exit code:" << ret;
	shvInfo() << "bye ...";

	return ret;
}
