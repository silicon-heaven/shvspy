#include "accessmodelshv3.h"

#include <shv/coreqt/log.h>
#include <shv/core/exception.h>
#include <shv/chainpack/rpcvalue.h>
#include <shv/iotqt/node/shvnode.h>
#include <shv/coreqt/rpc.h>

using namespace shv::chainpack;
using namespace std;

QString AccessModelShv3::columnName(int col)
{
	switch (col){
	case Columns::ColShvRI:
		return tr("Shv RI");
	case Columns::ColGrant:
		return tr("Grant");
	default:
		return tr("Unknown");
	}
}

AccessModelShv3::AccessModelShv3(QObject *parent)
	: Super(parent)
{
}

AccessModelShv3::~AccessModelShv3() = default;

void AccessModelShv3::setRules(const RpcValue &role_rules)
{
	/*
	{
	  "access":[
		{"grant":"wr", "shvRI":".broker/currentClient:subscribe"},
		{"grant":"wr", "shvRI":".broker/currentClient:unsubscribe"}
	  ],
	  "roles":[]
	}
	*/
	beginResetModel();
	m_rules.clear();
	for (const auto &rv : role_rules.asList()) {
		const auto &m = rv.asMap();
		m_rules << Rule {
				   .shvRI = m.value("shvRI").to<QString>(),
				   .grant = m.value("grant").to<QString>(),
		};
	}
	endResetModel();
}

RpcValue AccessModelShv3::rules()
{
	RpcValue::List rules;
	for (const auto &rule : m_rules) {
		RpcValue::Map rpcrule;
		rpcrule["shvRI"] = rule.shvRI.toStdString();
		rpcrule["grant"] = rule.grant.toStdString();
		rules.push_back(rpcrule);
	}
	return rules;
}

int AccessModelShv3::rowCount(const QModelIndex &parent) const
{
	Q_UNUSED(parent)
	return static_cast<int>(m_rules.size());
}

int AccessModelShv3::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid())
		return 0;

	return Columns::ColCount;
}

Qt::ItemFlags AccessModelShv3::flags(const QModelIndex &ix) const
{
	if (!ix.isValid()){
		return Qt::NoItemFlags;
	}

	return  Super::flags(ix) |= Qt::ItemIsEditable;
}

QVariant AccessModelShv3::data(const QModelIndex &ix, int role) const
{
	if (m_rules.empty() || ix.row() >= static_cast<int>(m_rules.size()) || ix.row() < 0){
		return QVariant();
	}

	const auto &rule = m_rules.at(ix.row());

	if(role == Qt::DisplayRole || role == Qt::EditRole) {
		switch (ix.column()) {
		case Columns::ColShvRI: return rule.shvRI;
		case Columns::ColGrant: return rule.grant;
		default: break;
		}
	}
	else if(role == Qt::ToolTipRole) {
		switch (ix.column()) {
		case Columns::ColShvRI:
			return tr("ShvRI of node, see: https://silicon-heaven.github.io/shv-doc/rpcri.html");
		case Columns::ColGrant:
			return tr("Comma separated list of grants, one must be convertible to access_level, see: https://silicon-heaven.github.io/shv-doc/rpcmessage.html.");
		default: break;
		}
	}

	return QVariant();
}

bool AccessModelShv3::setData(const QModelIndex &ix, const QVariant &val, int role)
{
	if (m_rules.empty() || ix.row() >= static_cast<int>(m_rules.size()) || ix.row() < 0)
		return false;

	if (role == Qt::EditRole){
		auto &rule = m_rules[ix.row()];
		if (ix.column() == Columns::ColShvRI) {
			rule.shvRI = val.toString();
			return true;
		}
		if (ix.column() == Columns::ColGrant) {
			rule.grant = val.toString();
			if (rule.grant.startsWith('"') && rule.grant.endsWith('"')) {
				rule.grant = rule.grant.mid(1, rule.grant.size() - 2);
			}
			return true;
		}
	}
	return false;
}

QVariant AccessModelShv3::headerData(int section, Qt::Orientation orientation, int role) const
{
	QVariant ret;
	if(orientation == Qt::Horizontal) {
		if(role == Qt::DisplayRole) {
			return columnName(section);
		}
	}

	return ret;
}

void AccessModelShv3::addRule()
{
	beginInsertRows(QModelIndex(), rowCount(), rowCount());
	m_rules << Rule{
			   .shvRI = "**:*:*",
			   .grant = "bws",
	};
	endInsertRows();
}

void AccessModelShv3::moveRuleUp(int index)
{
	if (index > 0) {
		beginMoveRows(QModelIndex(), index, index, QModelIndex(), index - 1);
		std::swap(m_rules[index], m_rules[index - 1]);
		endMoveRows();
	}
}

void AccessModelShv3::moveRuleDown(int index)
{
	if ((index >= 0) && (index < rowCount() - 1)) {
		beginMoveRows(QModelIndex(), index, index, QModelIndex(), index + 1);
		std::swap(m_rules[index], m_rules[index + 1]);
		endMoveRows();
	}
}

void AccessModelShv3::deleteRule(int index)
{
	if ((index >= 0) && (index < rowCount())){
		beginResetModel();
		m_rules.removeAt(index);
		endResetModel();
	}
}

bool AccessModelShv3::isRulesValid()
{
	for (int i = 0; i < rowCount(); i++){
		if (!m_rules[i].isValid())
			return false;
	}

	return true;
}


bool AccessModelShv3::Rule::isValid() const
{
	auto al = shv::iotqt::node::ShvNode::basicGrantToAccessLevel(grant.toStdString());
	if (al == shv::chainpack::AccessLevel::None) {
		return false;
	}
	return !shvRI.isEmpty();
}
