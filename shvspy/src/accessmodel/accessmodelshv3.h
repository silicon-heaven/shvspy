#pragma once

#include "accessmodel.h"

#include <shv/iotqt/acl/aclroleaccessrules.h>
#include <shv/chainpack/rpcvalue.h>

class AccessModelShv3 : public AccessModel
{
	Q_OBJECT
private:
	using Super = AccessModel;
public:
	enum Columns {ColShvRI = 0, ColGrant, ColCount};
	static QString columnName(int col);

public:
	AccessModelShv3(QObject *parent = nullptr);
	~AccessModelShv3() override;
public:
	void setRules(const shv::chainpack::RpcValue &role_paths) override;
	shv::chainpack::RpcValue rules() override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	Qt::ItemFlags flags(const QModelIndex &ix) const override;
	QVariant data( const QModelIndex &index, int role = Qt::DisplayRole ) const override;
	bool setData(const QModelIndex &ix, const QVariant &val, int role = Qt::EditRole) override;
	QVariant headerData (int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const override;
	void addRule() override;
	void moveRuleUp(int index) override;
	void moveRuleDown(int index) override;
	void deleteRule(int index) override;
	bool isRulesValid() override;

private:
	struct Rule {
		QString shvRI;
		QString grant; // comma separated list of grants, one must be convertible to access_level

		bool isValid() const;
	};
	QList<Rule> m_rules;
};
