#include "accessmodelshv2.h"

#include <shv/coreqt/log.h>
#include <shv/core/exception.h>
#include <shv/chainpack/rpcvalue.h>

#include <QBrush>

namespace cp = shv::chainpack;
using namespace std;

QString AccessModelShv2::columnName(int col)
{
	switch (col){
	case Columns::ColPath:
		return tr("Path");
	case Columns::ColMethod:
		return tr("Method");
	case Columns::ColAccess:
		return tr("Grant");
	default:
		return tr("Unknown");
	}
}

AccessModelShv2::AccessModelShv2(QObject *parent)
	: Super(parent)
{
}

AccessModelShv2::~AccessModelShv2() = default;

void AccessModelShv2::setRules(const cp::RpcValue &role_rules)
{
	beginResetModel();
	m_legacyRulesFormat = role_rules.isMap();
	m_rules = shv::iotqt::acl::AclRoleAccessRules::fromRpcValue(role_rules);
	endResetModel();
}

cp::RpcValue AccessModelShv2::rules()
{
	if(m_legacyRulesFormat)
		return m_rules.toRpcValue_legacy();
	return m_rules.toRpcValue();
}

int AccessModelShv2::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return static_cast<int>(m_rules.size());
}

int AccessModelShv2::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;

	return Columns::ColCount;
}

Qt::ItemFlags AccessModelShv2::flags(const QModelIndex &ix) const
{
	if (!ix.isValid()){
		return Qt::NoItemFlags;
	}

	return  Super::flags(ix) |= Qt::ItemIsEditable;
}

QVariant AccessModelShv2::data(const QModelIndex &ix, int role) const
{
	if (m_rules.empty() || ix.row() >= static_cast<int>(m_rules.size()) || ix.row() < 0){
		return QVariant();
	}

	const shv::iotqt::acl::AclAccessRule &rule = m_rules.at(ix.row());

	if(role == Qt::DisplayRole || role == Qt::EditRole) {
		switch (ix.column()) {
		case Columns::ColPath: return QString::fromStdString(rule.path);
		case Columns::ColMethod: return QString::fromStdString(rule.method);
		case Columns::ColAccess: return QString::fromStdString(rule.access);
		default: break;
		}
	}
	else if(role == Qt::ToolTipRole) {
		switch (ix.column()) {
		case Columns::ColPath:
			return tr("'path/to/node' or 'path/to/subtree/**' can be used here");
		case Columns::ColMethod:
			return tr("Rule is applied to this method only or to any method if blank.");
		default: break;
		}
	}

	return QVariant();
}

bool AccessModelShv2::setData(const QModelIndex &ix, const QVariant &val, int role)
{
	if (m_rules.empty() || ix.row() >= static_cast<int>(m_rules.size()) || ix.row() < 0)
		return false;

	if (role == Qt::EditRole){
		shv::iotqt::acl::AclAccessRule &rule = m_rules[ix.row()];
		if (ix.column() == Columns::ColPath) {
			rule.path = val.toString().toStdString();
			return true;
		}
		if (ix.column() == Columns::ColMethod) {
			rule.method = val.toString().toStdString();
			return true;
		}
		if (ix.column() == Columns::ColAccess) {
			rule.access = val.toString().toStdString();
			if (rule.access.size() >= 2 && rule.access[0] == '"' && rule.access[rule.access.size() - 1] == '"') {
				rule.access = rule.access.substr(1, rule.access.size() - 2);
			}
			return true;
		}
	}
	return false;
}

QVariant AccessModelShv2::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant ret;
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			return columnName(section);
		}
	}

	return ret;
}

void AccessModelShv2::addRule()
{
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	m_rules.push_back(shv::iotqt::acl::AclAccessRule());
	endInsertRows();
}

void AccessModelShv2::moveRuleUp(int index)
{
	if (index > 0) {
		beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1);
		std::swap(m_rules.at(index), m_rules.at(index - 1));
		endMoveRows();
	}
}

void AccessModelShv2::moveRuleDown(int index)
{
	if ((index >= 0) && (index < rowCount() - 1)) {
		beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 2);
		std::swap(m_rules.at(index), m_rules.at(index + 1));
		endMoveRows();
	}
}

void AccessModelShv2::deleteRule(int index)
{
	if ((index >= 0) && (index < rowCount())){
		beginResetModel();
		m_rules.erase(m_rules.begin() + index);
		endResetModel();
	}
}

bool AccessModelShv2::isRulesValid()
{
	for (int i = 0; i < rowCount(); i++){
		if (!m_rules[i].isValid())
			return false;
	}

	return true;
}

