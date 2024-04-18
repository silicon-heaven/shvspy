#include "subscriptionsmodel.h"

#include "../theapp.h"
#include "../servertreemodel/shvnodeitem.h"
#include "../servertreemodel/shvbrokernodeitem.h"
#include "../servertreemodel/servertreemodel.h"

#include <shv/chainpack/rpcvalue.h>
#include <shv/coreqt/log.h>

#include <QBrush>

namespace cp = shv::chainpack;

namespace {
auto constexpr KEY_Path = "Path";
auto constexpr KEY_Method = "Method";
auto constexpr KEY_Source = "Source";
auto constexpr KEY_IsPermanent = "IsPermanent";
auto constexpr KEY_IsEnabled = "IsEnabled";
}

SubscriptionsModel::SubscriptionsModel(QObject *parent)
	: Super(parent)
{
}

SubscriptionsModel::~SubscriptionsModel()
{
}

int SubscriptionsModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return static_cast<int>(m_subscriptions.count());
}

int SubscriptionsModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;

	return Columns::ColCount;
}

Qt::ItemFlags SubscriptionsModel::flags(const QModelIndex &ix) const
{
	if (!ix.isValid()){
		return Qt::NoItemFlags;
	}

	if(ix.column() == Columns::ColMethod || ix.column() == Columns::ColPath || ix.column() == Columns::ColSource){
		return  Super::flags(ix) |= Qt::ItemIsEditable;
	}

	if((ix.column() == Columns::ColEnabled) ||
		(ix.column() == Columns::ColPermanent)){
		return  Super::flags(ix) |= Qt::ItemIsUserCheckable;
	}

	return Super::flags(ix);
}

QVariant SubscriptionsModel::data(const QModelIndex &ix, int role) const
{
	if (m_subscriptions.isEmpty() || ix.row() >= m_subscriptions.count() || ix.row() < 0){
		return QVariant();
	}

	const Subscription &sub = m_subscriptions.at(ix.row());

	if(role == Qt::DisplayRole) {
		switch (ix.column()) {
		case Columns::ColServer:
			return sub.serverName();
		case Columns::ColPath:
			return sub.shvPath();
		case Columns::ColMethod:
			return sub.method();
		case Columns::ColSource:
			return sub.source();
		default: return {};
		}
	}
	if(role == Qt::CheckStateRole) {
		switch (ix.column()) {
		case Columns::ColPermanent:
			return (sub.isPermanent()) ? Qt::Checked : Qt::Unchecked;
		case Columns::ColEnabled:
			return (sub.isEnabled()) ? Qt::Checked : Qt::Unchecked;
		default: return {};
		}
	}
	if(role == Qt::UserRole) {
		if (ix.column() == Columns::ColServer){
			return sub.brokerId();
		}
		return {};
	}
	if(role == Qt::BackgroundRole) {
		if (ix.column() == Columns::ColServer) {
			return QBrush(QColor::fromHsv((sub.brokerId() * 36) % 360, 45, 255));
		}
		return {};
	}
	return {};
}

bool SubscriptionsModel::setData(const QModelIndex &ix, const QVariant &val, int role)
{
	if (m_subscriptions.isEmpty() || ix.row() >= m_subscriptions.count() || ix.row() < 0){
		return false;
	}

	if (role == Qt::CheckStateRole){
		Subscription &sub = m_subscriptions[ix.row()];
		int col = ix.column();
		bool v = (val == Qt::Checked) ? true : false;

		if (col == Columns::ColPermanent || col == Columns::ColEnabled){
			ShvBrokerNodeItem *nd = TheApp::instance()->serverTreeModel()->brokerById(sub.brokerId());
			if (nd == nullptr){
				return false;
			}

			if (col == Columns::ColPermanent){
				sub.setIsPermanent(v);
			}
			else if (col == Columns::ColEnabled){
				sub.setIsEnabled(v);
				nd->enableSubscription(sub.shvPath().toStdString(), sub.method().toStdString(), sub.source().toStdString(), v);
			}

			QVariantList new_subs = brokerSubscriptions(sub.brokerId());
			nd->setSubscriptionList(new_subs);

			return true;
		}
	}
	else if (role == Qt::EditRole) {
		auto update_subscription = [this, ix, val]() {
			Subscription &sub = m_subscriptions[ix.row()];

			ShvBrokerNodeItem *nd = TheApp::instance()->serverTreeModel()->brokerById(sub.brokerId());
			if (nd == nullptr) {
				return false;
			}

			nd->enableSubscription(sub.shvPath().toStdString(), sub.method().toStdString(), sub.source().toStdString(), false);
			switch (ix.column()) {
			case Columns::ColPath: sub.setShvPath(val.toString()); break;
			case Columns::ColMethod: sub.setMethod(val.toString()); break;
			case Columns::ColSource: sub.setSource(val.toString()); break;
			default: return false;
			}
			nd->enableSubscription(sub.shvPath().toStdString(), sub.method().toStdString(), sub.source().toStdString(), true);

			QVariantList new_subs = brokerSubscriptions(sub.brokerId());
			nd->setSubscriptionList(new_subs);
			emit dataChanged(ix.sibling(ix.row(), 0), ix.sibling(ix.row(), Columns::ColCount - 1));
			return true;
		};
		return update_subscription();
	}

	return false;
}

QVariant SubscriptionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant ret;
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			switch (section){
			case Columns::ColServer:
				return tr("Server");
			case Columns::ColPath:
				return tr("Path");
			case Columns::ColMethod:
				 return tr("Method");
			case Columns::ColSource:
				 return tr("Source");
			case Columns::ColPermanent:
				return tr("Permanent");
			case Columns::ColEnabled:
				return tr("Enabled");
			default:
				return tr("Unknown");
			}
		}
	}
	return ret;
}

void SubscriptionsModel::reload()
{
	beginResetModel();
	endResetModel();
}

void SubscriptionsModel::onBrokerConnectedChanged(int broker_id, bool is_connected)
{
	ShvBrokerNodeItem *nd = TheApp::instance()->serverTreeModel()->brokerById(broker_id);

	if (nd == nullptr){
		return;
	}

	beginResetModel();
	if (is_connected){
		QVariant v = nd->brokerProperties().value(ShvBrokerNodeItem::SUBSCRIPTIONS);

		if(v.isValid()) {
			QVariantList subs = v.toList();

			for (const auto & sub : subs) {
				if (sub.toMap().contains(KEY_IsPermanent)){
					SubscriptionsModel::Subscription s(broker_id, QString::fromStdString(nd->nodeId()));
					s.setConfig(sub.toMap());

					m_subscriptions.append(s);
				}
			}
		}
	}
	else{
		for (auto i = m_subscriptions.size() -1; i >= 0; i--) {
			if (m_subscriptions.at(i).brokerId() == broker_id){
				m_subscriptions.removeAt(i);
			}
		}
	}
	endResetModel();
}

void SubscriptionsModel::addSubscription(Subscription sub)
{
	int sub_ix = subscriptionIndex(sub.brokerId(), sub.shvPath(), sub.method());

	if (sub_ix == -1){
		beginInsertRows(QModelIndex(), static_cast<int>(m_subscriptions.count()), static_cast<int>(m_subscriptions.count()));
		m_subscriptions.append(sub);
		endInsertRows();
	}
	else{
		m_subscriptions[sub_ix].setIsEnabled(true);
		emit dataChanged(createIndex(sub_ix, Columns::ColEnabled), createIndex(sub_ix, Columns::ColEnabled), QVector<int>() << Qt::CheckStateRole);
	}
}

int SubscriptionsModel::subscriptionIndex(int broker_id, const QString &shv_path, const QString &method)
{
	int sub_ix = -1;
	for (int i = 0; i < m_subscriptions.count(); i++){
		const SubscriptionsModel::Subscription &s = m_subscriptions.at(i);
		if (s.brokerId() == broker_id && s.shvPath() == shv_path && s.method() == method){
			sub_ix = i;
			break;
		}
	}

	return sub_ix;
}

QVariantList SubscriptionsModel::brokerSubscriptions(int broker_id)
{
	QVariantList subscriptions;

	for (int i = 0; i < m_subscriptions.count(); i++){
		const Subscription &s = m_subscriptions.at(i);
		if (s.brokerId() == broker_id && s.isPermanent()){
			subscriptions.append(s.config());
		}
	}
	return  subscriptions;
}

SubscriptionsModel::Subscription::Subscription():
	SubscriptionsModel::Subscription(-1, QString())
{
}

SubscriptionsModel::Subscription::Subscription(int broker_id, const QString &server_name)
{
	m_brokerId = broker_id;
	m_serverName = server_name;
}

void SubscriptionsModel::Subscription::setConfig(const QVariantMap &config)
{
	m_config = config;
}

QVariantMap SubscriptionsModel::Subscription::config() const
{
	return m_config;
}

int SubscriptionsModel::Subscription::brokerId() const
{
	return m_brokerId;
}

QString SubscriptionsModel::Subscription::serverName() const
{
	return m_serverName;
}

QString SubscriptionsModel::Subscription::shvPath() const
{
	return m_config.value(KEY_Path).toString();
}

void SubscriptionsModel::Subscription::setShvPath(const QString &shv_path)
{
	m_config[KEY_Path] = shv_path;
}

QString SubscriptionsModel::Subscription::method() const
{
	return m_config.value(KEY_Method).toString();
}

void SubscriptionsModel::Subscription::setMethod(const QString &method)
{
	m_config[KEY_Method] = method;
}

QString SubscriptionsModel::Subscription::source() const
{
	return m_config.value(KEY_Source).toString();
}

void SubscriptionsModel::Subscription::setSource(const QString &source)
{
	m_config[KEY_Source] = source;
}

bool SubscriptionsModel::Subscription::isPermanent() const
{
	return m_config.value(KEY_IsPermanent).toBool();
}

void SubscriptionsModel::Subscription::setIsPermanent(bool val)
{
	m_config[KEY_IsPermanent] = val;
}

bool SubscriptionsModel::Subscription::isEnabled() const
{
	return m_config.value(KEY_IsEnabled).toBool();
}

void SubscriptionsModel::Subscription::setIsEnabled(bool val)
{
	m_config[KEY_IsEnabled] = val;
}
