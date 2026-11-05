#include "attributesmodel.h"

//#include "../theapp.h"
#include "../servertreemodel/shvnodeitem.h"
#include "../servertreemodel/shvbrokernodeitem.h"

#include <shv/chainpack/cponreader.h>
//#include <shv/chainpack/cponwriter.h>
#include <shv/chainpack/rpcvalue.h>
#include <shv/core/utils.h>
#include <shv/core/assert.h>
#include <shv/coreqt/log.h>
#include <shv/iotqt/rpc/clientconnection.h>
#include <shv/coreqt/rpc.h>

#include <QSettings>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QIcon>

namespace cp = shv::chainpack;

AttributesModel::AttributesModel(QObject *parent)
	: Super(parent)
{
}

int AttributesModel::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	if(m_shvTreeNodeItem.isNull())
		return 0;
	return static_cast<int>(m_shvTreeNodeItem->methods().count());
}

Qt::ItemFlags AttributesModel::flags(const QModelIndex &ix) const
{
	Qt::ItemFlags ret = Super::flags(ix);
	if(ix.column() == ColParams)
		ret |= Qt::ItemIsEditable;
	if(ix.column() == ColBtRun)
		ret &= ~Qt::ItemIsSelectable;
	return ret;
}

QVariant AttributesModel::data(const QModelIndex &ix, int role) const
{
	if(m_shvTreeNodeItem.isNull())
		return QVariant();
	const auto &mms = m_shvTreeNodeItem->methods();
	if(ix.row() < 0 || ix.row() >= mms.count())
		return QVariant();
	if(ix.column() < 0 || ix.column() >= ColCnt)
		return QVariant();

	const ShvMetaMethod &mm = mms[ix.row()];
	switch (role) {
	case Qt::DisplayRole: {
		switch (ix.column()) {
		case ColMethodName:
			return QString::fromStdString(mm.metamethod.name());
		case ColParamType:
			return QString::fromStdString(mm.metamethod.paramType());
		case ColResultType:
			return QString::fromStdString(mm.metamethod.resultType());
		case ColSignals:
			return QString::fromStdString(cp::RpcValue(mm.metamethod.methodSignals()).toCpon());
		case ColParams:
			return QString::fromStdString(mm.params.toCpon());
		case ColResult: {
			const auto &resp = mm.response;
			if (resp.isError()) {
				return QString::fromStdString(resp.error().message());
			}
			const auto &result = resp.result();
			if (!result.isValid()) {
					return QVariant();
			}
				static constexpr int MAX_TT_SIZE = 1024;
			std::string tts = result.toCpon();
			if (tts.size() > MAX_TT_SIZE) {
					tts = tts.substr(0, MAX_TT_SIZE) + " < ... " + std::to_string(tts.size() - MAX_TT_SIZE) + " more bytes >";
			}
			return QString::fromStdString(tts);
		}
		case ColFlags:
			return QString::fromStdString(mm.flagsStr());
		case ColAccessLevel:
			return QString::fromStdString(mm.accessLevelStr());
		default:
			break;
		}
		break;
	}
	case Qt::EditRole: {
		switch (ix.column()) {
		case ColResult:
		case ColParams:
			return data(ix, Qt::DisplayRole);
		default:
			break;
		}
		break;
	}
	case Qt::DecorationRole: {
		if(ix.column() == ColBtRun) {
			bool is_notify = mm.isSignal();
			if (!is_notify) {
				static QIcon ico_run = QIcon(QStringLiteral(":/shvspy/images/run"));
				static QIcon ico_reload = QIcon(QStringLiteral(":/shvspy/images/reload"));
				return (mm.rpcRequestId > 0)? ico_reload: ico_run;
			}
		}
		break;
	}
	case Qt::ToolTipRole: {
		if(ix.column() == ColBtRun) {
			return tr("Call remote method");
		}
		if(ix.column() == ColFlags) {
			return mm.isSignal() ? tr("Method is notify signal"): QVariant();
		}
		if(ix.column() == ColMethodName) {
			QStringList lines;
			for(const auto &[k, v] : mm.metamethod.toMap()) {
				lines << tr("%1: %2").arg(k.c_str(), v.toCpon().c_str());
			}
			return lines.join('\n');
		}
		return data(ix, Qt::DisplayRole);
	}
	default:
		break;
	}
	return QVariant();
}

bool AttributesModel::setData(const QModelIndex &ix, const QVariant &val, int role)
{
	shvLogFuncFrame() << val.toString() << val.typeName() << "role:" << role;
	if(role == Qt::EditRole) {
		if(ix.column() == ColParams) {
			if(!m_shvTreeNodeItem.isNull()) {
				std::string cpon = val.toString().trimmed().toStdString();
				cp::RpcValue params;
				if(!cpon.empty()) {
					std::string err;
					params = cp::RpcValue::fromCpon(cpon, &err);
					if(!err.empty())
						shvError() << "cannot set invalid cpon data:" << cpon << "error:" << err;
				}
				m_shvTreeNodeItem->setMethodParams(ix.row(), params);
				emit dataChanged(ix, ix);
				return true;
			}
		}
	}
	return false;
}

QVariant AttributesModel::headerData(int section, Qt::Orientation o, int role) const
{
	QVariant ret;
	if(o == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			if(section == ColMethodName)
				ret = tr("Method");
			else if(section == ColParamType)
				ret = tr("Param type");
			else if(section == ColResultType)
				ret = tr("Result type");
			else if(section == ColSignals)
				ret = tr("Signals");
			else if(section == ColFlags)
				ret = tr("Flags");
			else if(section == ColAccessLevel)
				ret = tr("ACG");
			else if(section == ColParams)
				ret = tr("Params");
			else if(section == ColResult)
				ret = tr("Response");
		}
		else if(role == Qt::ToolTipRole) {
			if(section == ColAccessLevel)
				ret = tr("Acess Grant");
		}
	}
	return ret;
}

void AttributesModel::load(ShvNodeItem *nd)
{
	beginResetModel();
	if(!m_shvTreeNodeItem.isNull())
		m_shvTreeNodeItem->disconnect(this);
	m_shvTreeNodeItem = nd;
	endResetModel();
	if(nd) {
		// Clazy false positive https://invent.kde.org/sdk/clazy/-/issues/22
		connect(nd, &ShvNodeItem::methodsLoaded, this, &AttributesModel::onMethodsLoaded, Qt::UniqueConnection); // clazy:exclude=lambda-unique-connection
		connect(nd, &ShvNodeItem::rpcMethodCallFinished, this, &AttributesModel::onRpcMethodCallFinished, Qt::UniqueConnection); // clazy:exclude=lambda-unique-connection
		if (nd->isMethodsLoaded()) {
			callGetters();
		}
		else if (!nd->isMethodsLoading()) {
			nd->loadMethods();
		}
	}
}

void AttributesModel::callMethod(unsigned method_ix, bool throw_exc)
{
	if(m_shvTreeNodeItem.isNull())
		return;
	m_shvTreeNodeItem->callMethod(method_ix, throw_exc);
	emit dataChanged(index(method_ix, ColResult), index(method_ix, ColBtRun));
}

QString AttributesModel::path() const
{
	if (m_shvTreeNodeItem.isNull()) {
		return QString();
	}
	return QString::fromStdString(m_shvTreeNodeItem->shvPath());
}

QString AttributesModel::methodName(int row) const
{
	if (m_shvTreeNodeItem.isNull() || row < 0 || row >= m_shvTreeNodeItem->methods().count()) {
		return QString();
	}
	return QString::fromStdString(m_shvTreeNodeItem->methods()[row].metamethod.name());
}

shv::chainpack::RpcValue AttributesModel::methodParams(int row) const
{
	if (m_shvTreeNodeItem.isNull() || row < 0 || row >= m_shvTreeNodeItem->methods().count()) {
		return {};
	}
	return m_shvTreeNodeItem->methods()[row].params;
}

shv::chainpack::RpcResponse AttributesModel::methodCallResult(int row) const
{
	if (m_shvTreeNodeItem.isNull() || row < 0 || row >= m_shvTreeNodeItem->methods().count()) {
		return {};
	}
	return m_shvTreeNodeItem->methods()[row].response;
}

shv::iotqt::rpc::ClientConnection *AttributesModel::clientConnection() const
{
	if (m_shvTreeNodeItem.isNull()) {
		return nullptr;
	}
	return m_shvTreeNodeItem->serverNode()->clientConnection();
}

void AttributesModel::onMethodsLoaded()
{
	beginResetModel();
	endResetModel();
	callGetters();
}

void AttributesModel::onRpcMethodCallFinished(int method_ix)
{
	emit dataChanged(index(method_ix, ColResult), index(method_ix, ColBtRun));
}

void AttributesModel::callGetters()
{
	if (m_shvTreeNodeItem.isNull()) {
		return;
	}
	const auto &mms = m_shvTreeNodeItem->methods();
	for (int i = 0; i < static_cast<int>(mms.count()); ++i) {
		const ShvMetaMethod &mm = mms[i];
		if (mm.isGetter() && !(mm.metamethod.flags() & cp::MetaMethod::Flag::LargeResultHint)) {
			callMethod(i);
		}
	}
}
