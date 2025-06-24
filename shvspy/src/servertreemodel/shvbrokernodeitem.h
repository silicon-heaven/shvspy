#pragma once

#include "shvnodeitem.h"

//#include <shv/chainpack/rpcvalue.h>

#include <map>

namespace shv::chainpack { class RpcValue; class RpcMessage; }
namespace shv::iotqt::rpc { class ClientConnection; }

class ShvBrokerNodeItem : public ShvNodeItem
{
	Q_OBJECT
private:
	using Super = ShvNodeItem;
public:
	static const QString SUBSCRIPTIONS;
	enum class OpenStatus {Invalid = 0, Disconnected, Connecting, Connected};
public:
	explicit ShvBrokerNodeItem(ServerTreeModel *m, const std::string &server_name, int rpc_timeout_msec);
	~ShvBrokerNodeItem() Q_DECL_OVERRIDE;

	QVariant data(int role = Qt::UserRole + 1) const Q_DECL_OVERRIDE;

	OpenStatus openStatus() const {return m_openStatus;}

	bool isOpen() const {return openStatus() == OpenStatus::Connected;}
	void open();
	void close();
	const std::string &shvRoot() const;

	QVariantMap brokerProperties() const;
	void setBrokerProperties(const QVariantMap &props);

	void setSubscriptionList(const QVariantList &subs);
	void addSubscription(const std::string &shv_path, const std::string &method, const std::string& source);
	void enableSubscription(const std::string &shv_path, const std::string &method, const std::string& source, bool is_enabled);

	shv::iotqt::rpc::ClientConnection *clientConnection();

	int callNodeRpcMethod(const std::string &calling_node_shv_path, const std::string &method, const shv::chainpack::RpcValue &params, bool throw_exc = false);

	ShvNodeItem *findNode(const std::string &path);

	int brokerId() const { return m_brokerId; }

	Q_SIGNAL void subscriptionAdded(const std::string &path, const std::string &method, const std::string& source);
	Q_SIGNAL void subscriptionAddError(const std::string &shv_path, const std::string &error_msg);

	Q_SIGNAL void brokerConnectedChange(bool is_connected);

private:
	void onBrokerConnectedChanged(bool is_connected);
	void onBrokerLoginError(const QString &err);
	void onRpcMessageReceived(const shv::chainpack::RpcMessage &msg);
	void checkShvApiVersion(QObject *context, std::function<void ()> on_success, std::function<void (const shv::chainpack::RpcError &e)> on_error = {});
	void createSubscriptions();
	int callSubscribe(const std::string &shv_path, const std::string &method, const std::string &source);
	int callUnsubscribe(const std::string &shv_path, const std::string &method, const std::string& source);
private:
	int m_brokerId;
	QVariantMap m_brokerPropeties;
	shv::iotqt::rpc::ClientConnection *m_rpcConnection = nullptr;
	OpenStatus m_openStatus = OpenStatus::Disconnected;
	struct RpcRequestInfo;
	std::map<int, RpcRequestInfo> m_runningRpcRequests;
	std::string m_shvRoot;
	int m_brokerLoginErrorCount = 0;
};

