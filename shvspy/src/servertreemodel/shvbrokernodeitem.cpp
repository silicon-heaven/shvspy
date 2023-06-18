#include "shvbrokernodeitem.h"
#include "servertreemodel.h"
#include "../brokerproperty.h"
#include "../theapp.h"
#include "../appclioptions.h"
#include "../log/rpcnotificationsmodel.h"
#include "../attributesmodel/attributesmodel.h"
#include "src/brokerproperty.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/iotqt/rpc/deviceconnection.h>
#include <shv/iotqt/rpc/deviceappclioptions.h>
#include <shv/iotqt/rpc/rpcresponsecallback.h>
#include <shv/iotqt/rpc/socket.h>
#include <shv/iotqt/node/shvnode.h>
#include <shv/core/utils/shvpath.h>
#include <shv/visu/errorlogmodel.h>

#include <shv/chainpack/rpcmessage.h>
#include <shv/core/stringview.h>
#include <shv/core/utils.h>
#include <shv/coreqt/log.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QIcon>
#include <QTimer>

namespace cp = shv::chainpack;

const QString ShvBrokerNodeItem::SUBSCRIPTIONS = QStringLiteral("subscriptions");

struct ShvBrokerNodeItem::RpcRequestInfo
{
	std::string shvPath;
	QElapsedTimer startTS;

	RpcRequestInfo()
	{
		startTS.start();
	}
};

ShvBrokerNodeItem::ShvBrokerNodeItem(ServerTreeModel *m, const std::string &server_name)
	: Super(m, server_name)
{
	static int s_broker_id = 0;
	m_brokerId = ++ s_broker_id;

	QTimer *rpc_rq_timeout = new QTimer(this);
	rpc_rq_timeout->start(5000);
	connect(rpc_rq_timeout, &QTimer::timeout, [this]() {
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


void ShvBrokerNodeItem::addSubscription(const std::string &shv_path, const std::string &method)
{
	int rqid = callSubscribe(shv_path, method);

	shv::iotqt::rpc::RpcResponseCallBack *cb = new shv::iotqt::rpc::RpcResponseCallBack(m_rpcConnection, rqid, this);
	cb->start(5000, this, [this, shv_path, method](const cp::RpcResponse &resp) {
		if(resp.isError() || (resp.result() == false)){
			emit subscriptionAddError(shv_path, resp.error().message());
		}
		else{
			emit subscriptionAdded(shv_path, method);
		}
	});
}

void ShvBrokerNodeItem::enableSubscription(const std::string &shv_path, const std::string &method, bool is_enabled)
{
	if (is_enabled){
		callSubscribe(shv_path, method);
	}
	else{
		callUnsubscribe(shv_path, method);
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
	if(scheme_enum == shv::iotqt::rpc::Socket::Scheme::LocalSocket || scheme_enum == shv::iotqt::rpc::Socket::Scheme::SerialPort) {
		host = scheme + ":" + host;
		cli->setLoginType(shv::iotqt::rpc::ClientConnection::LoginType::None);
	}
	else {
		host = scheme + "://" + host;
		if(port > 0)
			host += ':' + QString::number(port).toStdString();
		//cli->setLoginType(pwd.size() == 40? cp::IRpcConnection::LoginType::Sha1: cp::IRpcConnection::LoginType::Plain);
		if(scheme_enum == shv::iotqt::rpc::Socket::Scheme::Ssl) {
			// SSL encryption is enough
			// plain text password can be used for LDAP authentication on broker if enabled
			cli->setLoginType(cp::IRpcConnection::LoginType::Plain);
		}
		else {
			// do not send plain text password over not encrypted socket
			cli->setLoginType(cp::IRpcConnection::LoginType::Sha1);
		}
	}
	cli->setHost(host);
	//cli->setPort(m_serverPropeties.value("port").toInt());
	//cli->setSecurityType(m_serverPropeties.value("securityType").toString().toStdString());
	cli->setPeerVerify(m_brokerPropeties.value(brokerProperty::PEERVERIFY).toBool());
	cli->setUser(m_brokerPropeties.value(brokerProperty::USER).toString().toStdString());
	cli->setPassword(pwd);
	//cli->setSkipLoginPhase(m_brokerPropeties.value("skipLoginPhase").toBool());
	cli->open();
	emitDataChanged();
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
		{
			int proto_type = m_brokerPropeties.value(brokerProperty::RPC_PROTOCOLTYPE).toInt();
			if(proto_type == static_cast<int>(cp::Rpc::ProtocolType::JsonRpc))
				opts.setProtocolType("jsonrpc");
			else if(proto_type == static_cast<int>(cp::Rpc::ProtocolType::Cpon))
				opts.setProtocolType("cpon");
			else
				opts.setProtocolType("chainpack");
		}
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
		connect(m_rpcConnection, &shv::iotqt::rpc::ClientConnection::brokerLoginError, this, &ShvBrokerNodeItem::onBrokerLoginError);
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
		if(path.size() && path[0] == '/')
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

int ShvBrokerNodeItem::callSubscribe(const std::string &shv_path, std::string method)
{
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	int rqid = cc->callMethodSubscribe(shv_path, method);
	return rqid;
}

int ShvBrokerNodeItem::callUnsubscribe(const std::string &shv_path, std::string method)
{
	shv::iotqt::rpc::ClientConnection *cc = clientConnection();
	int rqid = cc->callMethodUnsubscribe(shv_path, method);
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
				else if(shv_path.empty()) {
					if(method == cp::Rpc::METH_DIR) {
						using namespace shv::chainpack;
						resp.setResult(cp::RpcValue::List{
										   MetaMethod(Rpc::METH_DIR, MetaMethod::Signature::RetParam, MetaMethod::Flag::None).toRpcValue(),
										   MetaMethod(Rpc::METH_APP_NAME, MetaMethod::Signature::RetVoid, MetaMethod::Flag::IsGetter).toRpcValue(),
										   MetaMethod(Rpc::METH_APP_VERSION, MetaMethod::Signature::RetVoid, MetaMethod::Flag::IsGetter).toRpcValue(),
										   MetaMethod(Rpc::METH_ECHO, MetaMethod::Signature::RetParam, MetaMethod::Flag::None, Rpc::ROLE_WRITE).toRpcValue(),
									   });
						break;
					}
					else if(method == cp::Rpc::METH_APP_NAME) {
						resp.setResult(QCoreApplication::instance()->applicationName().toStdString());
						break;
					}
					else if(method == cp::Rpc::METH_APP_VERSION) {
						resp.setResult(QCoreApplication::instance()->applicationVersion().toStdString());
						break;
					}
					else if(method == cp::Rpc::METH_ECHO) {
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
		m_rpcConnection->sendMessage(resp);
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
	QMetaEnum meta_sub = QMetaEnum::fromType<SubscriptionItem>();
	QVariant v = m_brokerPropeties.value(SUBSCRIPTIONS);
	if(v.isValid()) {
		QVariantList subs = v.toList();

		for (int i = 0; i < subs.size(); i++) {
			QVariantMap s = subs.at(i).toMap();

			if (s.value(meta_sub.valueToKey(SubscriptionItem::IsEnabled)).toBool()){
				callSubscribe(s.value(meta_sub.valueToKey(SubscriptionItem::Path)).toString().toStdString(), s.value(meta_sub.valueToKey(SubscriptionItem::Method)).toString().toStdString());
			}
		}
	}
}

