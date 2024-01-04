#pragma once

#include "shvnodeitem.h"

//#include <shv/chainpack/rpcvalue.h>

#include <map>

namespace shv { namespace chainpack { class RpcValue; class RpcMessage; } }
namespace shv { namespace iotqt { namespace rpc { class ClientConnection; } } }

class ShvBrokerNodeItem : public ShvNodeItem
{
	Q_OBJECT
private:
	using Super = ShvNodeItem;
public:
	static const QString SUBSCRIPTIONS;
	enum class OpenStatus {Invalid = 0, Disconnected, Connecting, Connected};
	enum SubscriptionItem {Path = 0, Method, IsPermanent, IsEnabled, Count};
	Q_ENUM(SubscriptionItem)
public:
	explicit ShvBrokerNodeItem(ServerTreeModel *m, const std::string &server_name);
	~ShvBrokerNodeItem() Q_DECL_OVERRIDE;

	QVariant data(int role = Qt::UserRole + 1) const Q_DECL_OVERRIDE;

	OpenStatus openStatus() const {return m_openStatus;}

	bool isOpen() {return openStatus() == OpenStatus::Connected;}
	void open();
	void close();
	//QString connectionErrorString();
	const std::string &shvRoot() const;

	QVariantMap brokerProperties() const;
	void setBrokerProperties(const QVariantMap &props);

	void setSubscriptionList(const QVariantList &subs);
	void addSubscription(const std::string &shv_path, const std::string &method);
	void enableSubscription(const std::string &shv_path, const std::string &method, bool is_enabled);

	shv::iotqt::rpc::ClientConnection *clientConnection();

	int callNodeRpcMethod(const std::string &calling_node_shv_path, const std::string &method, const shv::chainpack::RpcValue &params, bool throw_exc = false);

	ShvNodeItem *findNode(const std::string &path);

	int brokerId() const { return m_brokerId; }

	Q_SIGNAL void subscriptionAdded(const std::string &path, const std::string &method);
	Q_SIGNAL void subscriptionAddError(const std::string &shv_path, const std::string &error_msg);

	Q_SIGNAL void brokerConnectedChange(bool is_connected);

private:
	void onBrokerConnectedChanged(bool is_connected);
	void onBrokerLoginError(const QString &err);
	void onRpcMessageReceived(const shv::chainpack::RpcMessage &msg);
	void createSubscriptions();
	int callSubscribe(const std::string &shv_path, const std::string &method);
	int callUnsubscribe(const std::string &shv_path, const std::string &method);
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

