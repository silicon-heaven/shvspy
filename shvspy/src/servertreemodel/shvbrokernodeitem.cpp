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

ShvBrokerNodeItem::ShvBrokerNodeItem(ServerTreeModel *m, const std::string &server_name, int rpc_timeout_sec)
	: Super(m, server_name)
{
	static int s_broker_id = 0;
	m_brokerId = ++ s_broker_id;

	auto *rpc_rq_timeout = new QTimer(this);
	auto rpc_timeout_msec = rpc_timeout_sec * 1000;
	if (rpc_timeout_msec == 0) {
		rpc_timeout_msec = 5000;
	}
	rpc_rq_timeout->start(rpc_timeout_msec);
	connect(rpc_rq_timeout, &QTimer::timeout, this, [this, server_name, rpc_timeout_msec]() {
		QElapsedTimer tm2;
		tm2.start();
		auto it = m_runningRpcRequests.begin();
		while (it != m_runningRpcRequests.end()) {
			auto elapsed = it->second.startTS.msecsTo(tm2);
			if(elapsed > rpc_timeout_msec) {
				shvWarning() << "RPC request timeout expired for node:" << server_name << it->second.shvPath << "after:" << elapsed << "msecs.";
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
		if(resp.isError()){
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

	auto protocol_type = static_cast<shv::chainpack::Rpc::ProtocolType>(m_brokerPropeties.value(brokerProperty::RPC_PROTOCOLTYPE, static_cast<int>(shv::chainpack::Rpc::ProtocolType::ChainPack)).toInt());
	cli->setProtocolType(protocol_type);


#if QT_VERSION_MAJOR >= 6 && defined(WITH_AZURE_SUPPORT)
	bool azure_login = m_brokerPropeties.value(brokerProperty::AZURELOGIN, false).toBool();

	if (azure_login) {
		if (scheme_enum != shv::iotqt::rpc::Socket::Scheme::Ssl && scheme_enum != shv::iotqt::rpc::Socket::Scheme::WebSocketSecure) {
			QMessageBox::warning(nullptr, tr("Alert"), tr("Can't connect via Azure through an unencrypted connection. Please enable SSL."));
			m_openStatus = OpenStatus::Disconnected;
			return;
		}

		cli->setOauth2Azure(true);
		connect(cli, &shv::iotqt::rpc::ClientConnection::authorizeWithBrowser, this, [] (const auto& url) {
			QDesktopServices::openUrl(url);
		});
		cli->open();
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
	if(m_rpcConnection) {
		m_rpcConnection->close();
		m_rpcConnection->deleteLater();
		m_rpcConnection = nullptr;
	}
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
		checkShvApiVersion(this, [this]() { createSubscriptions(); });
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
		if (resp.delay().has_value()) {
			// ignore delay messages
			return;
		}
		if(resp.isError())
			TheApp::instance()->errorLogModel()->addLogRow(
						NecroLog::Level::Error
						, resp.error().message()
						, QString::fromStdString(cp::RpcResponse::Error::errorCodeToString(resp.error().code()))
						);
		int rqid = resp.requestId().toInt();
		auto it = m_runningRpcRequests.find(rqid);
		if(it == m_runningRpcRequests.end()) {
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
										   MetaMethod(Rpc::METH_APP_NAME, MetaMethod::Flag::IsGetter, {}, "String").toRpcValue(),
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

void ShvBrokerNodeItem::checkShvApiVersion(QObject *context, std::function<void ()> on_success, std::function<void (const shv::chainpack::RpcError &)> on_error)
{
	auto *rpc_call = shv::iotqt::rpc::RpcCall::create(m_rpcConnection)->setShvPath(".broker")->setMethod("ls");
	connect(rpc_call, &shv::iotqt::rpc::RpcCall::maybeResult, context, [this, on_success, on_error](const ::shv::chainpack::RpcValue &result, const shv::chainpack::RpcError &error) {
		if (error.isValid()) {
			if (on_error) {
				on_error(error);
			}
			else {
				shvError() << "SHV API version discovery error:" << error.toString();
			}
		}
		else {
			using ShvApiVersion = shv::iotqt::rpc::ClientConnection::ShvApiVersion;
			const auto &dirs = result.asList();
			if (std::find(dirs.begin(), dirs.end(), "app") != dirs.end()) {
				shvInfo() << "Setting SHV API to ver 2";
				m_rpcConnection->setShvApiVersion(ShvApiVersion::V2);
				on_success();
			}
			else if (std::find(dirs.begin(), dirs.end(), "client") != dirs.end()) {
				shvInfo() << "Setting SHV API to ver 3";
				m_rpcConnection->setShvApiVersion(ShvApiVersion::V3);
				on_success();
			}
			else {
				if (on_error) {
					on_error(error);
				}
				else {
					shvError() << "SHV API version cannot be discovered from ls result.";
				}
			}
		}
	});
	rpc_call->start();
}

void ShvBrokerNodeItem::createSubscriptions()
{
	using namespace shv::iotqt::rpc;
	using namespace shv::chainpack;
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	auto *rpc = RpcCall::create(cc);
	rpc->setShvPath(Rpc::DIR_BROKER_CURRENTCLIENT)
			->setMethod(Rpc::METH_DIR)
			->setParams(Rpc::METH_SUBSCRIBE);
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

