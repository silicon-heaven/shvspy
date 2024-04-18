#include "shvbrokernodeitem.h"
#include "servertreemodel.h"
#include "../brokerproperty.h"
#include "../theapp.h"
#include "../appclioptions.h"
#include "../log/rpcnotificationsmodel.h"
#include "../attributesmodel/attributesmodel.h"
#include "../subscriptionsmodel/subscriptionsmodel.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/deviceconnection.h>
#include <shv/iotqt/rpc/deviceappclioptions.h>
#include <shv/iotqt/rpc/rpccall.h>
#include <shv/iotqt/rpc/socket.h>
#include <shv/iotqt/node/shvnode.h>
#include <shv/core/utils/shvpath.h>
#include <shv/visu/errorlogmodel.h>

#include <shv/chainpack/rpc.h>
#include <shv/chainpack/rpcmessage.h>
#include <shv/core/stringview.h>
#include <shv/core/utils.h>
#include <shv/coreqt/log.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>

#if QT_VERSION_MAJOR >= 6 && defined(WITH_AZURE_SUPPORT)
#include <QDesktopServices>
#include <QFuture>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#endif

namespace cp = shv::chainpack;

const QString ShvBrokerNodeItem::SUBSCRIPTIONS = QStringLiteral("subscriptions");

struct ShvBrokerNodeItem::RpcRequestInfo
{
	std::string shvPath;
	QElapsedTimer startTS;

	RpcRequestInfo() {
		startTS.start();
	}
};

ShvBrokerNodeItem::ShvBrokerNodeItem(ServerTreeModel *m, const std::string &server_name)
	: Super(m, server_name)
{
	static int s_broker_id = 0;
	m_brokerId = ++ s_broker_id;

	auto *rpc_rq_timeout = new QTimer(this);
	rpc_rq_timeout->start(5000);
	connect(rpc_rq_timeout, &QTimer::timeout, this, [this]() {
		QElapsedTimer tm2;
		tm2.start();
		auto it = m_runningRpcRequests.begin();
		while (it != m_runningRpcRequests.end()) {
			if(it->second.startTS.msecsTo(tm2) > shv::iotqt::rpc::ClientConnection::defaultRpcTimeoutMsec()) {
				shvWarning() << "RPC request timeout expired for node:" << it->second.shvPath;
				it = m_runningRpcRequests.erase(it);
			}
			else
				++it;
		}
	});
}

ShvBrokerNodeItem::~ShvBrokerNodeItem()
{
	if(m_rpcConnection) {
		disconnect(m_rpcConnection, nullptr, this, nullptr);
		delete m_rpcConnection;
	}
}

QVariant ShvBrokerNodeItem::data(int role) const
{
	QVariant ret;
	if(role == Qt::DisplayRole) {
		ret = QString::fromStdString(nodeId());
		//if(m_clientConnection) {
		//	ret = m_clientConnection->serverName();
		//}
	}
	else if(role == Qt::DecorationRole) {
		static QIcon ico_connected = QIcon(QStringLiteral(":/shvspy/images/connected.png"));
		static QIcon ico_connecting = QIcon(QStringLiteral(":/shvspy/images/connecting.png"));
		static QIcon ico_disconnected = QIcon(QStringLiteral(":/shvspy/images/disconnected.png"));
		switch (openStatus()) {
		case OpenStatus::Connected: return ico_connected;
		case OpenStatus::Connecting: return ico_connecting;
		case OpenStatus::Disconnected: return ico_disconnected;
		default: return QIcon();
		}
	}
	else
		ret = Super::data(role);
	return ret;
}

QVariantMap ShvBrokerNodeItem::brokerProperties() const
{
	return m_brokerPropeties;
}

void ShvBrokerNodeItem::setSubscriptionList(const QVariantList &subs)
{
	m_brokerPropeties[brokerProperty::SUBSCRIPTIONS] = subs;
}


void ShvBrokerNodeItem::addSubscription(const std::string &shv_path, const std::string &method, const std::string &source)
{
	int rqid = callSubscribe(shv_path, method, source);
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);
	cb->start(5000, this, [this, shv_path, method, source](const cp::RpcResponse &resp) {
		if(resp.isError() || (resp.result() == false)){
			emit subscriptionAddError(shv_path, resp.error().message());
		}
		else{
			emit subscriptionAdded(shv_path, method, source);
		}
	});
}

void ShvBrokerNodeItem::enableSubscription(const std::string &shv_path, const std::string &method, const std::string &source, bool is_enabled)
{
	if (is_enabled){
		callSubscribe(shv_path, method, source);
	}
	else{
		callUnsubscribe(shv_path, method, source);
	}
}

void ShvBrokerNodeItem::setBrokerProperties(const QVariantMap &props)
{
	if(m_rpcConnection) {
		delete m_rpcConnection;
		m_rpcConnection = nullptr;
	}
	m_brokerPropeties = props;
	setNodeId(m_brokerPropeties.value(brokerProperty::NAME).toString().toStdString());
	m_shvRoot = m_brokerPropeties.value(brokerProperty::SHVROOT).toString().toStdString();
}

const std::string& ShvBrokerNodeItem::shvRoot() const
{
	return m_shvRoot;
}

#if QT_VERSION_MAJOR >= 6 && defined(WITH_AZURE_SUPPORT)
namespace {
const auto AZURE_CLIENT_ID = "f6c73b47-914d-4232-bbe9-70495e48b314";
const auto AZURE_AUTH_URL = QUrl("https://login.microsoftonline.com/common/oauth2/v2.0/authorize");
const auto AZURE_ACCESS_TOKEN_URL = QUrl("https://login.microsoftonline.com/common/oauth2/v2.0/token");
const auto AZURE_SCOPES = "User.Read";

#define azureInfo() shvCInfo("azure")
#define azureWarning() shvCWarning("azure")
#define azureError() shvCError("azure")

auto do_azure_auth(QObject* parent)
{
	auto oauth2 = new QOAuth2AuthorizationCodeFlow(parent);
	auto replyHandler = new QOAuthHttpServerReplyHandler(oauth2);
	replyHandler->setCallbackText("Azure authentication successful. You can now close this window.");
	oauth2->setReplyHandler(replyHandler);
	oauth2->setClientIdentifier(AZURE_CLIENT_ID);
	oauth2->setAuthorizationUrl(AZURE_AUTH_URL);
	oauth2->setAccessTokenUrl(AZURE_ACCESS_TOKEN_URL);
	oauth2->setScope(AZURE_SCOPES);
	QObject::connect(oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, [] (const auto& url) {
		QDesktopServices::openUrl(url);
	});

	QObject::connect(oauth2, &QOAuth2AuthorizationCodeFlow::statusChanged, [] (const QOAuth2AuthorizationCodeFlow::Status& status) {
		azureInfo() << "Status changed: " << [status] {
			using Status = QOAuth2AuthorizationCodeFlow::Status;
			switch (status) {
			case Status::NotAuthenticated: return "NotAuthenticated";
			case Status::TemporaryCredentialsReceived: return "TemporaryCredentialsReceived";
			case Status::Granted: return "Granted";
			case Status::RefreshingToken: return "RefreshingToken";
			}
			throw std::logic_error{"QOAuth2AuthorizationCodeFlow::statusChanged: unknown status" + std::to_string(static_cast<int>(status))};
		}();
	});

	oauth2->grant();
	return QtFuture::whenAny(
		QtFuture::connect(oauth2, &QOAuth2AuthorizationCodeFlow::granted).then([oauth2] {
		return oauth2->token();
	}), QtFuture::connect(oauth2, &QOAuth2AuthorizationCodeFlow::error).then([] (std::tuple<const QString&, const QString&, const QUrl&> errors) {
		auto [error, error_description, error_url] = errors;
		auto res = QStringLiteral("Failed to authenticate with Azure.\n");
		auto append_error_if_not_empty = [&res] (const auto& error_prefix, const auto& error_str) {
			if (!error_str.isEmpty()) {
				res += error_prefix;
				res += error_str;
				res += '\n';
			}
		};
		append_error_if_not_empty("Error: ", error);
		// The error description is a percent encoded string with plus signs instead of spaces.
		append_error_if_not_empty("Description: ", QUrl::fromPercentEncoding(error_description.toLatin1().replace('+', ' ')));
		// The error url is actually inside the path the QUrl. Urgh. The reason is that Azure precent-encodes the URL,
		// and Qt doesn't expect that.
		append_error_if_not_empty("URL: ", error_url.path());
		azureWarning() << res;
		return res;
	})).then([oauth2] (const std::variant<QFuture<QString>, QFuture<QString>>& result) {
		oauth2->deleteLater();
		return result;
	});
}
}
#endif

void ShvBrokerNodeItem::open()
{
	close();
	m_brokerLoginErrorCount = 0;
	m_openStatus = OpenStatus::Connecting;
	shv::iotqt::rpc::ClientConnection *cli = clientConnection();
	//cli->setServerName(props.value("name").toString());
	//cli->setScheme(m_serverPropeties.value("scheme").toString().toStdString());
	auto scheme = m_brokerPropeties.value(brokerProperty::SCHEME).toString().toStdString();
	auto scheme_enum = shv::iotqt::rpc::Socket::schemeFromString(scheme);
	if(scheme_enum == shv::iotqt::rpc::Socket::Scheme::Tcp && m_brokerPropeties.value(brokerProperty::SECURITYTYPE).toString() == "SSL")
		scheme_enum = shv::iotqt::rpc::Socket::Scheme::Ssl;
	scheme = shv::iotqt::rpc::Socket::schemeToString(scheme_enum);
	auto host = m_brokerPropeties.value(brokerProperty::HOST).toString().toStdString();
	auto port = m_brokerPropeties.value(brokerProperty::PORT).toInt();
	std::string pwd = m_brokerPropeties.value(brokerProperty::PASSWORD).toString().toStdString();
	if(scheme_enum == shv::iotqt::rpc::Socket::Scheme::LocalSocket || scheme_enum == shv::iotqt::rpc::Socket::Scheme::LocalSocketSerial || scheme_enum == shv::iotqt::rpc::Socket::Scheme::SerialPort) {
		host = scheme + ":" + host;
	}
	else {
		host = scheme + "://" + host;
		if(port > 0) {
			host += ':' + QString::number(port).toStdString();
		}
	}
	bool skip_login = m_brokerPropeties.value(brokerProperty::SKIPLOGINPHASE).toBool();
	if (skip_login) {
		cli->setLoginType(shv::iotqt::rpc::ClientConnection::LoginType::None);
	}
	else {
		//cli->setLoginType(pwd.size() == 40? cp::IRpcConnection::LoginType::Sha1: cp::IRpcConnection::LoginType::Plain);
		if(scheme_enum == shv::iotqt::rpc::Socket::Scheme::Ssl) {
			// SSL encryption is enough
			// plain text password can be used for LDAP authentication on broker if enabled
			cli->setLoginType(cp::IRpcConnection::LoginType::Plain);
		}
		else {
			if(m_brokerPropeties.value(brokerProperty::PLAIN_TEXT_PASSWORD).toBool()) {
				cli->setLoginType(cp::IRpcConnection::LoginType::Plain);
			}
			else {
				// do not send plain text password over not encrypted socket
				cli->setLoginType(cp::IRpcConnection::LoginType::Sha1);
			}
		}
	}
	cli->setHost(host);
	//cli->setPort(m_serverPropeties.value("port").toInt());
	//cli->setSecurityType(m_serverPropeties.value("securityType").toString().toStdString());
	cli->setPeerVerify(m_brokerPropeties.value(brokerProperty::PEERVERIFY).toBool());
	cli->setUser(m_brokerPropeties.value(brokerProperty::USER).toString().toStdString());
	cli->setPassword(pwd);
	//cli->setSkipLoginPhase(m_brokerPropeties.value("skipLoginPhase").toBool());

#if QT_VERSION_MAJOR >= 6 && defined(WITH_AZURE_SUPPORT)
	bool azure_login = m_brokerPropeties.value(brokerProperty::AZURELOGIN, false).toBool();

	if (azure_login) {
		if (scheme_enum != shv::iotqt::rpc::Socket::Scheme::Ssl) {
			QMessageBox::warning(nullptr, tr("Alert"), tr("Can't connect via Azure through an unencrypted connection. Please enable SSL."));
			m_openStatus = OpenStatus::Disconnected;
			return;
		}
		do_azure_auth(this).then([this, cli] (const std::variant<QFuture<QString>, QFuture<QString>>& result_or_error) {
			if (result_or_error.index() == 0) {
				auto result = std::get<0>(result_or_error);
				// This can happen due to a bug: https://bugreports.qt.io/browse/QTBUG-115580
				if (!result.isValid()) {
					return;
				}
				cli->setLoginType(shv::iotqt::rpc::ClientConnection::LoginType::AzureAccessToken);
				cli->setPassword(std::get<0>(result_or_error).result().toStdString());
				cli->open();
				emitDataChanged();
			}
			else {
				auto error = std::get<1>(result_or_error);
				// This can happen due to a bug: https://bugreports.qt.io/browse/QTBUG-115580
				if (!error.isValid()) {
					return;
				}
				onBrokerLoginError(std::get<1>(result_or_error).result());
			}
		});
	}
	else {
#endif
	cli->open();
	emitDataChanged();
#if QT_VERSION_MAJOR >= 6 && defined(WITH_AZURE_SUPPORT)
	}
#endif
}

void ShvBrokerNodeItem::close()
{
	//if(openStatus() == OpenStatus::Disconnected)
	//	return;
	if(m_rpcConnection)
		m_rpcConnection->close();
	m_openStatus = OpenStatus::Disconnected;
	deleteChildren();
	emitDataChanged();
}

shv::iotqt::rpc::ClientConnection *ShvBrokerNodeItem::clientConnection()
{
	if(!m_rpcConnection) {
		QString conn_type = m_brokerPropeties.value(brokerProperty::CONNECTIONTYPE).toString();

		shv::iotqt::rpc::DeviceAppCliOptions opts;
		//{
		//	int proto_type = m_brokerPropeties.value(brokerProperty::RPC_PROTOCOLTYPE).toInt();
		//	if(proto_type == static_cast<int>(cp::Rpc::ProtocolType::JsonRpc))
		//		opts.setProtocolType("jsonrpc");
		//	else if(proto_type == static_cast<int>(cp::Rpc::ProtocolType::Cpon))
		//		opts.setProtocolType("cpon");
		//	else
		//		opts.setProtocolType("chainpack");
		//}
		{
			QVariant v = m_brokerPropeties.value(brokerProperty::RPC_RECONNECTINTERVAL);
			if(v.isValid())
				opts.setReconnectInterval(v.toInt());
		}
		{
			QVariant v = m_brokerPropeties.value(brokerProperty::RPC_HEARTBEATINTERVAL);
			if(v.isValid())
				opts.setHeartBeatInterval(v.toInt());
		}
		{
			QVariant v = m_brokerPropeties.value(brokerProperty::RPC_RPCTIMEOUT);
			if(v.isValid())
				opts.setRpcTimeout(v.toInt());
		}
		{
			QString dev_id = m_brokerPropeties.value(brokerProperty::DEVICE_ID).toString();
			if(!dev_id.isEmpty())
				opts.setDeviceId(dev_id.toStdString());
		}
		{
			QString mount_point = m_brokerPropeties.value(brokerProperty::DEVICE_MOUNTPOINT).toString();
			if(!mount_point.isEmpty())
				opts.setMountPoint(mount_point.toStdString());
		}
		if(conn_type == "device") {
			auto *c = new shv::iotqt::rpc::DeviceConnection(nullptr);
			c->setCliOptions(&opts);
			m_rpcConnection = c;
		}
		else {
			m_rpcConnection = new shv::iotqt::rpc::ClientConnection(nullptr);
			m_rpcConnection->setCliOptions(&opts);
		}
		if(brokerProperties().value(brokerProperty::MUTEHEARTBEATS).toBool()) {
			m_rpcConnection->muteShvPathInLog(shv::chainpack::Rpc::DIR_BROKER_APP, shv::chainpack::Rpc::METH_PING);
		}
		m_rpcConnection->setRawRpcMessageLog(TheApp::instance()->cliOptions()->isRawRpcMessageLog());
		//m_rpcConnection->setCheckBrokerConnectedInterval(0);
		connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::brokerConnectedChanged, this, &ShvBrokerNodeItem::onBrokerConnectedChanged);
		connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::rpcMessageReceived, this, &ShvBrokerNodeItem::onRpcMessageReceived);
		connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::brokerLoginError, this, [this](const shv::chainpack::RpcError &err) {
			auto err_msg = QString::fromStdString(err.toString());
			onBrokerLoginError(err_msg);
		});
		connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::socketError, this, &ShvBrokerNodeItem::onBrokerLoginError);
	}
	return m_rpcConnection;
}

void ShvBrokerNodeItem::onBrokerConnectedChanged(bool is_connected)
{
	m_openStatus = is_connected? OpenStatus::Connected: OpenStatus::Disconnected;
	emitDataChanged();
	if(is_connected) {
		createSubscriptions();
		loadChildren();
		AttributesModel *m = TheApp::instance()->attributesModel();
		m->load(this);
	}
	else {
		// do not close connection when connection drops
		// we can test this way device auto-reconnect even in shvspy
		// where it does not make a much sense
		//close();
	}

	emit brokerConnectedChange(is_connected);
}

void ShvBrokerNodeItem::onBrokerLoginError(const QString &err)
{
	emit treeModel()->brokerLoginError(brokerId(), err, ++m_brokerLoginErrorCount);
}

ShvNodeItem* ShvBrokerNodeItem::findNode(const std::string &path_)
{
	shvLogFuncFrame() << path_ << "shv root:" << shvRoot();
	ShvNodeItem *ret = this;
	std::string path = path_;
	if(!shvRoot().empty()) {
		path = path.substr(shvRoot().size());
		if(!path.empty() && path[0] == '/')
			path = path.substr(1);
	}
	shv::core::StringViewList id_list = shv::core::utils::ShvPath::split(path);
	for(const shv::core::StringView &node_id : id_list) {
		int i;
		auto row_cnt = ret->childCount();
		for (i = 0; i < row_cnt; ++i) {
			ShvNodeItem *nd = ret->childAt(i);
			if(nd && node_id == nd->nodeId()) {
				ret = nd;
				break;
			}
		}
		if(i == row_cnt) {
			return nullptr;
		}
	}
	return ret;
}

int ShvBrokerNodeItem::callSubscribe(const std::string &shv_path, const std::string &method, const std::string& source)
{
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	int rqid = cc->callMethodSubscribe(shv_path, method, source);
	return rqid;
}

int ShvBrokerNodeItem::callUnsubscribe(const std::string &shv_path, const std::string &method, const std::string& source)
{
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	int rqid = cc->callMethodUnsubscribe(shv_path, method, source);
	return rqid;
}

int ShvBrokerNodeItem::callNodeRpcMethod(const std::string &calling_node_shv_path, const std::string &method, const cp::RpcValue &params, bool throw_exc)
{
	shvLogFuncFrame() << calling_node_shv_path;
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	if(throw_exc && !cc->isBrokerConnected())
		SHV_EXCEPTION("Broker is not connected.");
	int rqid = cc->callShvMethod(calling_node_shv_path, method, params);
	m_runningRpcRequests[rqid].shvPath = calling_node_shv_path;
	return rqid;
}

void ShvBrokerNodeItem::onRpcMessageReceived(const shv::chainpack::RpcMessage &msg)
{
	if(msg.isResponse()) {
		cp::RpcResponse resp(msg);
		if(resp.isError())
			TheApp::instance()->errorLogModel()->addLogRow(
						NecroLog::Level::Error
						, resp.error().message()
						, QString::fromStdString(cp::RpcResponse::Error::errorCodeToString(resp.error().code()))
						);
		int rqid = resp.requestId().toInt();
		auto it = m_runningRpcRequests.find(rqid);
		if(it == m_runningRpcRequests.end()) {
			//shvWarning() << "unexpected request id:" << rqid;
			// can be load attributes request
			return;
		}
		const std::string &path = it->second.shvPath;
		ShvNodeItem *nd = findNode(path);
		if(nd) {
			nd->processRpcMessage(msg);
		}
		else {
			shvError() << "Running RPC request response arrived - cannot find node on path:" << path;
		}
		m_runningRpcRequests.erase(it);
	}
	else if(msg.isRequest()) {
		cp::RpcRequest rq(msg);
		cp::RpcResponse resp = cp::RpcResponse::forRequest(rq);
		try {
			//shvInfo() << "RPC request received:" << rq.toCpon();
			do {
				const auto shv_path = rq.shvPath().asString();
				const auto method = rq.method().asString();
				if(shv_path == shv::chainpack::Rpc::DIR_BROKER_APP) {
						resp.setResult(true);
						break;
				}
				if(shv_path.empty()) {
					if(method == cp::Rpc::METH_DIR) {
						using namespace shv::chainpack;
						resp.setResult(cp::RpcValue::List{
										   shv::chainpack::methods::DIR.toRpcValue(),
										   shv::chainpack::methods::LS.toRpcValue(),
										   MetaMethod(Rpc::METH_APP_VERSION, MetaMethod::Flag::IsGetter, {}, "String").toRpcValue(),
										   MetaMethod(Rpc::METH_ECHO, MetaMethod::Flag::None, "RpcValue", "RpcValue", AccessLevel::Write).toRpcValue(),
									   });
						break;
					}
					if(method == cp::Rpc::METH_APP_NAME) {
						resp.setResult(QCoreApplication::applicationName().toStdString());
						break;
					}
					if(method == cp::Rpc::METH_APP_VERSION) {
						resp.setResult(QCoreApplication::applicationVersion().toStdString());
						break;
					}
					if(method == cp::Rpc::METH_ECHO) {
						resp.setResult(rq.params());
						break;
					}
				}
				SHV_EXCEPTION("Invalid method: " + method + " on path: " + shv_path);
			} while (false);
		}
		catch (shv::core::Exception &e) {
			resp.setError(cp::RpcResponse::Error::create(cp::RpcResponse::Error::MethodCallException, e.message()));
		}
		m_rpcConnection->sendRpcMessage(resp);
	}
	else if(msg.isSignal()) {
		shvDebug() << msg.toCpon();
		if(brokerProperties().value(brokerProperty::MUTEHEARTBEATS).toBool()) {
			if(msg.method().asString() == "appserver.heartBeat")
				return;
		}
		RpcNotificationsModel *m = TheApp::instance()->rpcNotificationsModel();
		m->addLogRow(nodeId(), msg);
	}
}

void ShvBrokerNodeItem::createSubscriptions()
{
	using namespace shv::iotqt::rpc;
	using namespace shv::chainpack;
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	auto *rpc = RpcCall::create(cc);
	rpc->setShvPath(Rpc::DIR_APP)
			->setMethod(Rpc::METH_LS)
			->setParams("broker");
	connect(rpc, &RpcCall::maybeResult, this, [this](const auto &result, const auto &err) {
		using ShvApiVersion = shv::iotqt::rpc::ClientConnection::ShvApiVersion;
		if(err.isValid()) {
			clientConnection()->setShvApiVersion(ShvApiVersion::V2);
		}
		else {
			clientConnection()->setShvApiVersion(result.toBool()? ShvApiVersion::V3: ShvApiVersion::V2);
		}

		QVariant v = m_brokerPropeties.value(SUBSCRIPTIONS);
		if(v.isValid()) {
			QVariantList subs = v.toList();

			for (const auto & sub : subs) {
				QVariantMap config = sub.toMap();
				SubscriptionsModel::Subscription subscription;
				subscription.setConfig(config);
				if (subscription.isEnabled()) {
					callSubscribe(subscription.shvPath().toStdString(), subscription.method().toStdString(), subscription.source().toStdString());
				}
			}
		}
	});
	rpc->start();
}

