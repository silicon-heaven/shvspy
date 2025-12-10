#include "mainwindow.h"
#include "theapp.h"
#include "appversion.h"
#include "appclioptions.h"

#include <shv/chainpack/rpcmessage.h>
#include <shv/core/utils.h>
#include <shv/coreqt/log.h>
#include <shv/coreqt/utils.h>

#include <QTextStream>
#include <QDateTime>
#include <QSettings>
#include <QFile>

#include <iostream>

#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/val.h>

#include <QUrl>
#include <QUrlQuery>
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

	std::vector<std::string> shv_args = {"shvspy"};

	emscripten::val location = emscripten::val::global("location");
	auto urlstr = QString::fromStdString(location["href"].as<std::string>());
	QUrl url(urlstr);
	shvInfo() << "url string:" << urlstr;
	QUrlQuery q(url);
	shvInfo() << "url:" << url.toString();
	for (const auto &[k, v] : q.queryItems()) {
		auto arg = QStringLiteral("--%1").arg(k).toStdString();
		shvInfo() << "Adding CLI arg:" << arg << v;
		shv_args.push_back(arg);
		if (!v.isEmpty()) {
			shv_args.push_back(v.toStdString());
		}
	}
#else
	std::vector<std::string> shv_args = NecroLog::setCLIOptions(argc, argv);
#endif

	int ret = 0;
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
