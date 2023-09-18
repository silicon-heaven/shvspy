#include "servertreemodel.h"
#include "shvbrokernodeitem.h"

#include "../theapp.h"

#include <shv/core/utils.h>
#include <shv/coreqt/log.h>
#include <shv/core/assert.h>
#include <shv/iotqt/rpc/deviceconnection.h>

#include <QSettings>
#include <QJsonDocument>
#include <QJsonParseError>
//#include <QDebug>

ServerTreeModel::ServerTreeModel(QObject *parent)
	: Super(parent)
{
	m_invisibleRoot = new ShvNodeRootItem(this);
}

ServerTreeModel::~ServerTreeModel()
{
	delete m_invisibleRoot;
}

QModelIndex ServerTreeModel::index(int row, int column, const QModelIndex &parent) const
{
	ShvNodeItem *pnd = itemFromIndex(parent);
	if(!pnd || column > 0)
		return QModelIndex();
	if(row < pnd->childCount()) {
		ShvNodeItem *nd = pnd->childAt(row);
		return createIndex(row, 0, nd->modelId());
	}
	return QModelIndex();
}

QModelIndex ServerTreeModel::parent(const QModelIndex &child) const
{
	ShvNodeItem *nd = itemFromIndex(child);
	if(nd)
		return indexFromItem(nd->parentNode());
	return QModelIndex();
}

ShvBrokerNodeItem *ServerTreeModel::createConnection(const QVariantMap &params)
{
	auto *ret = new ShvBrokerNodeItem(this, params.value("name").toString().toStdString());

	connect(ret, &ShvBrokerNodeItem::subscriptionAdded, this, [this, ret](const std::string &path, const std::string &method){
		emit ServerTreeModel::subscriptionAdded(ret->brokerId(), path, method);
	});

	connect(ret, &ShvBrokerNodeItem::subscriptionAddError, this, [this, ret](const std::string &path, const std::string &error_msg){
		emit ServerTreeModel::subscriptionAddError(ret->brokerId(), path, error_msg);
	});

	connect(ret, &ShvBrokerNodeItem::brokerConnectedChange, this, [this, ret](bool is_connected){
		emit ServerTreeModel::brokerConnectedChanged(ret->brokerId(), is_connected);
	});

	const std::string broker_name = ret->nodeId();
	ret->setBrokerProperties(params);
	ShvNodeRootItem *root = invisibleRootItem();
	root->appendChild(ret);
	return ret;
}
/*
ServerNode *ServerTreeModel::connectionForOid(int oid)
{
	ServerNode *ret = nullptr;
	QStandardItem *root = invisibleRootItem();
	for(int i=0; i<root->rowCount(); i++) {
		ServerNode *nd = dynamic_cast<ServerNode*>(root->child(i));
		SHV_ASSERT_EX(nd != nullptr, "Internal error");
		if(nd->oid() == oid) {
			ret = nd;
			break;
		}
	}
	return ret;
}
*/
int ServerTreeModel::columnCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return 1;
}

bool ServerTreeModel::hasChildren(const QModelIndex &parent) const
{
	ShvNodeItem *par_nd = itemFromIndex(parent);
	if(par_nd) {
		if(par_nd->hasChildren().isValid())
			return par_nd->hasChildren().toBool();
	}
	return rowCount(parent) > 0;
}

int ServerTreeModel::rowCount(const QModelIndex &parent) const
{
	ShvNodeItem *par_nd = itemFromIndex(parent);
	auto *par_brnd = qobject_cast<ShvBrokerNodeItem*>(par_nd);
	//shvDebug() << "ServerTreeModel::rowCount, item:" << par_it << "parent model index valid:" << parent.isValid();
	if(par_brnd || par_nd == invisibleRootItem()) {
		return static_cast<int>(par_nd->childCount());
	}
	if(par_nd) {
		//shvDebug() << "\t parent node:" << par_nd << "id:" << par_nd->nodeId() << "path:" << par_nd->shvPath();
		if(!par_nd->isChildrenLoaded() && !par_nd->isChildrenLoading()) {
			shvDebug() << "\t loading" << par_nd->shvPath();
			par_nd->loadChildren();
		}
		return static_cast<int>(par_nd->childCount());
	}
	return 0;
}

QVariant ServerTreeModel::data(const QModelIndex &ix, int role) const
{
	ShvNodeItem *nd = itemFromIndex(ix);
	if(nd)
		return nd->data(role);
	return QVariant();
}

QVariant ServerTreeModel::headerData(int section, Qt::Orientation o, int role) const
{
	Q_UNUSED(section)
	if(o == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			return tr("Node");
		}
	}
	return QVariant();
}

ShvNodeItem *ServerTreeModel::itemFromIndex(const QModelIndex &ix) const
{
	if(!ix.isValid())
		return m_invisibleRoot;
	ShvNodeItem *nd = m_nodes.value(static_cast<unsigned>(ix.internalId()));
	return nd;
}

QModelIndex ServerTreeModel::indexFromItem(ShvNodeItem *nd) const
{
	if(!nd || nd == invisibleRootItem())
		return QModelIndex();
	ShvNodeItem *pnd = nd->parentNode();
	if(!pnd)
		return QModelIndex();
	unsigned model_id = nd->modelId();
	int row;
	for (row = 0; row < pnd->childCount(); ++row) {
		ShvNodeItem *cnd = pnd->childAt(row);
		if(cnd == nd)
			break;
	}
	if(row < pnd->childCount())
		return createIndex(row, 0, model_id);
	return QModelIndex();
}

ShvBrokerNodeItem *ServerTreeModel::brokerById(int id)
{
	for (int i = 0; i < m_invisibleRoot->childCount(); i++){
		auto *nd = qobject_cast<ShvBrokerNodeItem*>(m_invisibleRoot->childAt(i));
		SHV_ASSERT_EX(nd != nullptr, "Internal error");
		if (nd->brokerId() == id){
			return  nd;
		}
	}
	return nullptr;
}

void ServerTreeModel::loadSettings(const QSettings &settings)
{
	QString servers_json = settings.value("application/servers").toString();
	std::string err;
	shv::chainpack::RpcValue rv = shv::chainpack::RpcValue::fromCpon(servers_json.toStdString(), &err);
	if(!err.empty()) {
		shvError() << "Erorr parse server settings:" << err;
		return;
	}
	loadSettings(rv);
}

void ServerTreeModel::loadSettings(const shv::chainpack::RpcValue &settings)
{
	for(const auto &rv : settings.asList()) {
		QVariantMap m = shv::coreqt::Utils::rpcValueToQVariant(rv).toMap();
		m["password"] = QString::fromStdString(TheApp::instance()->crypt().decrypt(m.value("password").toString().toStdString()));
		createConnection(m);
	}
}

void ServerTreeModel::saveSettings(QSettings &settings) const
{
	ShvNodeRootItem *root = invisibleRootItem();
	QVariantList lst;
	for(int i=0; i<root->childCount(); i++) {
		auto *nd = qobject_cast<ShvBrokerNodeItem*>(root->childAt(i));
		SHV_ASSERT_EX(nd != nullptr, "Internal error");
		QVariantMap props = nd->brokerProperties();
		props["password"] = QString::fromStdString(TheApp::instance()->crypt().encrypt(props.value("password").toString().toStdString(), 30));
		lst << props;
	}
	QJsonDocument jsd = QJsonDocument::fromVariant(lst);
	QString servers_json = QString::fromUtf8(jsd.toJson(QJsonDocument::Compact));
	settings.setValue("application/servers", servers_json);
}
