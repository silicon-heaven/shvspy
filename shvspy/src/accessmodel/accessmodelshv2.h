#pragma once

#include "accessmodel.h"

#include <shv/iotqt/acl/aclroleaccessrules.h>
#include <shv/chainpack/rpcvalue.h>

class AccessModelShv2 : public AccessModel
{
	Q_OBJECT
private:
	using Super = AccessModel;
public:
	enum Columns {ColPath = 0, ColMethod, ColAccess, ColCount};
	static QString columnName(int col);

public:
	AccessModelShv2(QObject *parent = nullptr);
	~AccessModelShv2() override;
public:
	void setRules(const shv::chainpack::RpcValue &role_paths);
	shv::chainpack::RpcValue rules();

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	Qt::ItemFlags flags(const QModelIndex &ix) const override;
	QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
	bool setData(const QModelIndex &ix, const QVariant &val, int role = Qt::EditRole) override;
	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
	void addRule();
	void moveRuleUp(int index);
	void moveRuleDown(int index);
	void deleteRule(int index);
	bool isRulesValid();

private:
	shv::iotqt::acl::AclRoleAccessRules m_rules;
	bool m_legacyRulesFormat = false;
};
