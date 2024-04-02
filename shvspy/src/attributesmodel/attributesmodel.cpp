#include "attributesmodel.h"

//#include "../theapp.h"
#include "../servertreemodel/shvnodeitem.h"

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
	const QVector<ShvMetaMethod> &mms = m_shvTreeNodeItem->methods();
	if(ix.row() < 0 || ix.row() >= mms.count())
		return QVariant();
	if(ix.column() < 0 || ix.column() >= ColCnt)
		return QVariant();

	switch (role) {
	case Qt::DisplayRole: {
		switch (ix.column()) {
		case ColMethodName:
		case ColParamType:
		case ColResultType: {
			cp::RpcValue rv = m_rows[ix.row()][ix.column()];
			return rv.isValid()? QString::fromStdString(rv.asString()): QVariant();
		}
		case ColParams: {
			//QVariant v = m_rows.value(ix.row()).value(ix.column());
			cp::RpcValue rv = m_rows[ix.row()][ix.column()];
			return rv.isValid()? QString::fromStdString(rv.toCpon()): QVariant();
		}
		case ColResult: {
			if(m_rows[ix.row()][ColError].isIMap()) {
				auto err = cp::RpcResponse::Error::fromRpcValue(m_rows[ix.row()][ColError].asIMap());
				return QString::fromStdString(err.message());
			}
			{
				cp::RpcValue rv = m_rows[ix.row()][ColResult];
				if(!rv.isValid())
					return QVariant();

				static constexpr int MAX_TT_SIZE = 1024;
				std::string tts = rv.toCpon();
				if(tts.size() > MAX_TT_SIZE)
					tts = tts.substr(0, MAX_TT_SIZE) + " < ... " + std::to_string(tts.size() - MAX_TT_SIZE) + " more bytes >";

				//shvWarning() << tts << tts.length();
				return QString::fromStdString(tts);
			}
		}
		case ColFlags:
		case ColAccessLevel:
			return QString::fromStdString(m_rows[ix.row()][ix.column()].toString());
		default:
			break;
		}
		break;
	}
	case Qt::EditRole: {
		switch (ix.column()) {
		case ColResult:
		case ColParams: {
			cp::RpcValue rv = m_rows[ix.row()][ix.column()];
			return rv.isValid()? QString::fromStdString(rv.toCpon()): QVariant();
		}
		default:
			break;
		}
		break;
	}
	case RpcValueRole: {
		switch (ix.column()) {
		case ColResult:
		case ColParams: {
			cp::RpcValue rv = m_rows[ix.row()][ix.column()];
			return QVariant::fromValue(rv);
		}
		default:
			break;
		}
		break;
	}
	case Qt::DecorationRole: {
		if(ix.column() == ColBtRun) {
			bool is_notify = m_rows[ix.row()][ColFlags].toBool();
			if(!is_notify) {
				static QIcon ico_run = QIcon(QStringLiteral(":/shvspy/images/run"));
				static QIcon ico_reload = QIcon(QStringLiteral(":/shvspy/images/reload"));
				auto v = m_rows[ix.row()][ColBtRun];
				return (v.toUInt() > 0)? ico_reload: ico_run;
			}
		}
		break;
	}
	case Qt::ToolTipRole: {
		if(ix.column() == ColBtRun) {
			return tr("Call remote method");
		}
		if(ix.column() == ColResult) {
			return data(ix, Qt::DisplayRole);
		}
		if(ix.column() == ColFlags) {
			bool is_notify = m_rows[static_cast<unsigned>(ix.row())][ColFlags].toUInt() & cp::MetaMethod::Flag::IsSignal;
			return is_notify? tr("Method is notify signal"): QVariant();
		}
		if(ix.column() == ColMethodName) {
			auto attrs = m_rows[static_cast<unsigned>(ix.row())][ColAttributes].asMap();
			QStringList lines;
			for(const auto &kv : attrs) {
				lines << tr("%1: %2").arg(kv.first.c_str()).arg(kv.second.toCpon().c_str());
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
				loadRow(ix.row());
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
			else if(section == ColFlags)
				ret = tr("Flags");
			else if(section == ColAccessLevel)
				ret = tr("ACG");
			else if(section == ColParams)
				ret = tr("Params");
			else if(section == ColResult)
				ret = tr("Result");
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
	m_rows.clear();
	if(!m_shvTreeNodeItem.isNull())
		m_shvTreeNodeItem->disconnect(this);
	m_shvTreeNodeItem = nd;
	if(nd) {
		connect(nd, &ShvNodeItem::methodsLoaded, this, &AttributesModel::onMethodsLoaded, Qt::UniqueConnection);
		connect(nd, &ShvNodeItem::rpcMethodCallFinished, this, &AttributesModel::onRpcMethodCallFinished, Qt::UniqueConnection);
		nd->checkMethodsLoaded();
	}
	loadRows();
}

void AttributesModel::callMethod(unsigned method_ix, bool throw_exc)
{
	if(m_shvTreeNodeItem.isNull())
		return;
	unsigned rqid = m_shvTreeNodeItem->callMethod(method_ix, throw_exc);
	m_rows[method_ix][ColBtRun] = rqid;
	emitRowChanged(method_ix);
}

QString AttributesModel::path() const
{
	if (m_shvTreeNodeItem.isNull()) {
		return QString();
	}
	return QString::fromStdString(m_shvTreeNodeItem->shvPath());
}

QString AttributesModel::method(int row) const
{
	if (m_shvTreeNodeItem.isNull()) {
		return QString();
	}
	return QString::fromStdString(m_shvTreeNodeItem->methods()[row].metamethod.name());
}

void AttributesModel::onMethodsLoaded()
{
	loadRows();
}

void AttributesModel::onRpcMethodCallFinished(int method_ix)
{
	loadRow(method_ix);
	emitRowChanged(method_ix);
	emit methodCallResultChanged(method_ix);
}

void AttributesModel::emitRowChanged(int row_ix)
{
	QModelIndex ix1 = index(row_ix, 0);
	QModelIndex ix2 = index(row_ix, ColCnt - 1);
	emit dataChanged(ix1, ix2);
}

void AttributesModel::callGetters()
{
	for (unsigned i = 0; i < m_rows.size(); ++i) {
		const ShvMetaMethod *mm = metaMethodAt(i);
		if(mm) {
			if((mm->metamethod.flags() & cp::MetaMethod::Flag::IsGetter) && !(mm->metamethod.flags() & cp::MetaMethod::Flag::LargeResultHint)) {
				callMethod(i);
			}
		}
	}
}

const ShvMetaMethod *AttributesModel::metaMethodAt(unsigned method_ix)
{
	if(method_ix >= m_rows.size() || m_shvTreeNodeItem.isNull())
		return nullptr;
	const QVector<ShvMetaMethod> &mm = m_shvTreeNodeItem->methods();
	if(method_ix >= static_cast<unsigned>(mm.size()))
		return nullptr;
	return &(mm[method_ix]);
}

void AttributesModel::loadRow(unsigned method_ix)
{
	const ShvMetaMethod * mtd = metaMethodAt(method_ix);
	if(!mtd)
		return;
	RowVals &rv = m_rows[method_ix];
	shvDebug() << "load row:" << mtd->metamethod.name() << "flags:" << mtd->metamethod.flags() << mtd->flagsStr();
	rv[ColMethodName] = mtd->metamethod.name();
	rv[ColResultType] = mtd->metamethod.resultType();
	rv[ColParamType] = mtd->metamethod.paramType();
	rv[ColFlags] = mtd->flagsStr();
	rv[ColAccessLevel] = mtd->accessLevelStr();
	rv[ColParams] = mtd->params;
	rv[ColAttributes] = mtd->metamethod.toRpcValue();
	shvDebug() << "\t response:" << mtd->response.toCpon() << "is valid:" << mtd->response.isValid();
	if(mtd->response.isError()) {
		rv[ColResult] = mtd->response.error().toRpcValue();
	}
	else {
		shv::chainpack::RpcValue result = mtd->response.result();
		rv[ColResult] = result;
	}
	rv[ColBtRun] = mtd->rpcRequestId;
}

void AttributesModel::loadRows()
{
	m_rows.clear();
	if(!m_shvTreeNodeItem.isNull()) {
		const QVector<ShvMetaMethod> &mm = m_shvTreeNodeItem->methods();
		for (int i = 0; i < mm.count(); ++i) {
			RowVals rv;
			rv.resize(ColCnt);
			m_rows.push_back(rv);
			loadRow(static_cast<unsigned>(m_rows.size() - 1));
		}
	}
	emit layoutChanged();
	emit reloaded();
	callGetters();
}

/*
void AttributesModel::onRpcMessageReceived(const shv::chainpack::RpcMessage &msg)
{
	if(msg.isResponse()) {
		cp::RpcResponse resp(msg);
		if(resp.requestId() == m_rpcRqId) {
			for(const cp::RpcValue &val : resp.result().toList()) {
				appendRow(QList<QStandardItem*>{
							  new QStandardItem(QString::fromStdString(val.toString())),
							  new QStandardItem("<not called>"),
						  });
			}
		}
	}
}
*/
