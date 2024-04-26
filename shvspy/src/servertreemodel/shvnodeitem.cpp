#include "shvnodeitem.h"
#include "shvbrokernodeitem.h"
#include "servertreemodel.h"
//#include "../theapp.h"

#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/chainpack/cponreader.h>
#include <shv/chainpack/cponwriter.h>
#include <shv/core/utils/shvpath.h>

#include <shv/core/assert.h>

#include <QIcon>
#include <QVariant>

using namespace shv::chainpack;

std::string ShvMetaMethod::flagsStr() const
{
	std::string ret;
	if(metamethod.flags() & MetaMethod::Flag::IsSignal)
		ret += (ret.empty() ? std::string() : std::string(",")) + std::string("SIG");
	if(metamethod.flags() & MetaMethod::Flag::IsGetter)
		ret += (ret.empty() ? std::string() : std::string(",")) + std::string("G");
	if(metamethod.flags() & MetaMethod::Flag::IsSetter)
		ret += (ret.empty()? std::string() : std::string(",")) + std::string("S");
	return ret;
}

std::string ShvMetaMethod::accessLevelStr() const
{
	std::string s = accessLevelToAccessString(metamethod.accessLevel());
	if (s.empty()) {
		s = std::to_string(static_cast<int>(metamethod.accessLevel()));
	}
	return s;
}

bool ShvMetaMethod::isSignal() const
{
	return metamethod.flags() & MetaMethod::Flag::IsSignal;
}

bool ShvMetaMethod::isGetter() const
{
	return metamethod.flags() & MetaMethod::Flag::IsGetter;
}

ShvNodeItem::ShvNodeItem(ServerTreeModel *m, const std::string &ndid, ShvNodeItem *parent)
	: Super(parent)
	, m_nodeId(ndid)
	, m_treeModelId(m->nextId())
{
	setObjectName(QString::fromStdString(ndid));
	m->m_nodes[m_treeModelId] = this;
}

ShvNodeItem::~ShvNodeItem()
{
	treeModel()->m_nodes.remove(m_treeModelId);
}

ServerTreeModel *ShvNodeItem::treeModel() const
{
	for(QObject *o = this->parent(); o; o = o->parent()) {
		auto *m = qobject_cast<ServerTreeModel*>(o);
		if(m)
			return m;
	}
	SHV_EXCEPTION("ServerTreeModel parent must exist.");
}

QVariant ShvNodeItem::data(int role) const
{
	QVariant ret;
	if(role == Qt::DisplayRole) {
		ret = objectName();
	}
	else if(role == Qt::BackgroundRole) {
		if(objectName() == QStringLiteral(".local"))
			return QColor(QStringLiteral("yellowgreen"));
	}
	else if(role == Qt::DecorationRole) {
		if(isChildrenLoading()) {
			static QIcon ico_reload = QIcon(QStringLiteral(":/shvspy/images/reload"));
			ret = ico_reload;
		}
	}
	return ret;
}

ShvBrokerNodeItem *ShvNodeItem::serverNode()
{
	for(auto *nd = this; nd; nd = nd->parentNode()) {
		auto *bnd = qobject_cast<ShvBrokerNodeItem *>(nd);
		if(bnd)
			return bnd;
	}
	SHV_EXCEPTION("ServerNode parent must exist.");
}

const ShvBrokerNodeItem *ShvNodeItem::serverNode() const
{
	for(auto *nd = this; nd; nd = nd->parentNode()) {
		auto *bnd = qobject_cast<const ShvBrokerNodeItem *>(nd);
		if(bnd)
			return bnd;
	}
	SHV_EXCEPTION("ServerNode parent must exist.");
}

ShvNodeItem *ShvNodeItem::parentNode() const
{
	return qobject_cast<ShvNodeItem*>(parent());
}

ShvNodeItem *ShvNodeItem::childAt(qsizetype ix) const
{
	if(ix < 0 || ix >= m_children.count())
		SHV_EXCEPTION("Invalid child index");
	return m_children[ix];
}

ShvNodeItem *ShvNodeItem::childAt(const std::string_view &id) const
{
	for (qsizetype i = 0; i < childCount(); ++i) {
		ShvNodeItem *nd = childAt(i);
		if(nd && id == nd->nodeId()) {
			return nd;
		}
	}
	return nullptr;
}

void ShvNodeItem::insertChild(qsizetype ix, ShvNodeItem *n)
{
	ServerTreeModel *m = treeModel();
	m->beginInsertRows(m->indexFromItem(this), static_cast<int>(ix), static_cast<int>(ix));
	n->setParent(this);
	m_children.insert(ix, n);
	m->endInsertRows();
}

void ShvNodeItem::deleteChild(int ix)
{
	ShvNodeItem *ret = childAt(ix);
	if(ret) {
		ServerTreeModel *m = treeModel();
		m->beginRemoveRows(m->indexFromItem(this), ix, ix);
		m_children.remove(ix);
		delete ret;
		m->endRemoveRows();
	}
}

void ShvNodeItem::deleteChildren()
{
	if(childCount() == 0)
		return;
	ServerTreeModel *m = treeModel();
	m->beginRemoveRows(m->indexFromItem(this), 0, static_cast<int>(childCount() - 1));
	qDeleteAll(m_children);
	m_children.clear();
	m->endRemoveRows();
	emitDataChanged();
}

std::string ShvNodeItem::shvPath() const
{
	std::vector<std::string> lst;
	auto *srv_nd = serverNode();
	const ShvNodeItem *nd = this;
	while(nd) {
		if(!nd || nd == srv_nd)
			break;
		lst.push_back(nd->nodeId());
		nd = nd->parentNode();
	}
	std::reverse(lst.begin(), lst.end());
	std::string path = shv::core::utils::ShvPath::joinDirs(lst);
	path = shv::core::utils::joinPath(srv_nd->shvRoot(), path);
	return path;
}

void ShvNodeItem::processRpcMessage(const shv::chainpack::RpcMessage &msg)
{
	if(msg.isResponse()) {
		RpcResponse resp(msg);
		int rqid = resp.requestId().toInt();
		if(rqid == m_loadChildrenRqId) {
			m_loadChildrenRqId = 0;
			m_childrenLoaded = true;

			deleteChildren();
			ServerTreeModel *m = treeModel();
			const auto res = resp.result();
			for(const auto &dir_entry : res.asList()) {
				std::string ndid = dir_entry.asString();
				auto *nd = new ShvNodeItem(m, ndid);
				//if(!long_dir_entry.empty()) {
				//	RpcValue has_children = long_dir_entry.value(1);
				//	if(has_children.isBool()) {
				//		nd->setHasChildren(has_children.toBool());
				//		if(!has_children.toBool())
				//			nd->setChildrenLoaded();
				//	}
				//}
				appendChild(nd);
			}
			emitDataChanged();
			emit childrenLoaded();
		}
		else if(rqid == m_dirRqId) {
			m_dirRqId = 0;
			m_methodsLoaded = true;

			m_methods.clear();
			RpcValue methods = resp.result();
			for(const RpcValue &method : methods.asList()) {
				ShvMetaMethod mm;
				mm.metamethod = shv::chainpack::MetaMethod::fromRpcValue(method);
				m_methods.push_back(mm);
			}
			emit methodsLoaded();
		}
		else {
			for (int i = 0; i < m_methods.count(); ++i) {
				ShvMetaMethod &mtd = m_methods[i];
				if(mtd.rpcRequestId == rqid) {
					mtd.rpcRequestId = 0;
					mtd.response = resp;
					emit rpcMethodCallFinished(i);
					break;
				}
			}
		}
	}
}

void ShvNodeItem::emitDataChanged()
{
	ServerTreeModel *m = treeModel();
	QModelIndex ix = m->indexFromItem(this);
	emit m->dataChanged(ix, ix);
}

void ShvNodeItem::loadChildren()
{
	m_childrenLoaded = false;
	ShvBrokerNodeItem *srv_nd = serverNode();
	m_loadChildrenRqId = srv_nd->callNodeRpcMethod(shvPath(), Rpc::METH_LS, {});
	//emitDataChanged();
}

bool ShvNodeItem::checkMethodsLoaded()
{
	if(!isMethodsLoaded() && !isMethodsLoading()) {
		loadMethods();
		return false;
	}
	return true;
}

void ShvNodeItem::loadMethods()
{
	m_methodsLoaded = false;
	ShvBrokerNodeItem *srv_nd = serverNode();
	m_dirRqId = srv_nd->callNodeRpcMethod(shvPath(), Rpc::METH_DIR, RpcValue::List{std::string(), true});
}

void ShvNodeItem::setMethodParams(int method_ix, const shv::chainpack::RpcValue &params)
{
	if(method_ix < 0 || method_ix >= m_methods.count())
		return;
	ShvMetaMethod &mtd = m_methods[method_ix];
	mtd.params = params;
}

unsigned ShvNodeItem::callMethod(int method_ix, bool throw_exc)
{
	//const QVector<ShvMetaMethod> &mm = m_methods();
	if(method_ix < 0 || method_ix >= m_methods.count())
		return 0;
	ShvMetaMethod &mtd = m_methods[method_ix];
	if(mtd.metamethod.name().empty() || (mtd.isSignal() && !mtd.isGetter()))
		return 0;
	mtd.response = RpcResponse();
	ShvBrokerNodeItem *srv_nd = serverNode();
	mtd.rpcRequestId = srv_nd->callNodeRpcMethod(shvPath(), mtd.metamethod.name(), mtd.params, throw_exc);
	return mtd.rpcRequestId;
}

void ShvNodeItem::reload()
{
	deleteChildren();
	m_methods.clear();
	loadChildren();
	loadMethods();
	emitDataChanged();
}

ShvNodeRootItem::ShvNodeRootItem(ServerTreeModel *parent)
	: Super(parent, std::string(), nullptr)
{
	setParent(parent);
}

