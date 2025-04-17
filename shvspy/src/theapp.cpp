#include "theapp.h"
#include "servertreemodel/servertreemodel.h"
#include "attributesmodel/attributesmodel.h"
#include "log/rpcnotificationsmodel.h"

#include <shv/coreqt/log.h>
#include <shv/visu/errorlogmodel.h>

#include <QSettings>
#ifdef Q_OS_WIN
#include <QStyleFactory>
#endif

TheApp::TheApp(int &argc, char **argv, AppCliOptions *cli_opts)
	: Super(argc, argv)
	, m_cliOptions(cli_opts)
	, m_crypt(shv::core::utils::Crypt::createGenerator(17456, 3148, 2147483647))
{
#ifdef Q_OS_WIN
	// set default style to fusion to overcome ugly look on some Windows installations
	for(auto key : QStyleFactory::keys()) {
		if(key == QLatin1String("Fusion")) {
			setStyle(QStyleFactory::create(key));
			break;
		}
	}
#endif
	m_serverTreeModel = new ServerTreeModel(this);
	m_attributesModel = new AttributesModel(this);
	m_rpcNotificationsModel = new RpcNotificationsModel(this);
	m_errorLogModel = new shv::visu::ErrorLogModel(this);
}

TheApp::~TheApp() = default;

void TheApp::saveSettings(QSettings &settings)
{
	m_serverTreeModel->saveSettings(settings);
}

std::string TheApp::aclAccessPath(const std::string &broker_path, shv::iotqt::rpc::ClientConnection::ShvApiVersion api_version)
{
	switch (api_version) {
	case shv::chainpack::IRpcConnection::ShvApiVersion::V2: return broker_path + "/etc/acl";
	case shv::chainpack::IRpcConnection::ShvApiVersion::V3: return broker_path + "/access";
	}
	return "";
}
