#pragma once

#include <QAbstractTableModel>

namespace shv::chainpack { class RpcValue; }

class AccessModel : public QAbstractTableModel
{
	Q_OBJECT
private:
	typedef QAbstractTableModel Super;
public:
	AccessModel(QObject *parent = nullptr);
	~AccessModel() override = default;
public:
	virtual void setRules(const shv::chainpack::RpcValue &role_paths) = 0;
	virtual shv::chainpack::RpcValue rules() = 0;

	virtual void addRule() = 0;
	virtual void moveRuleUp(int index) = 0;
	virtual void moveRuleDown(int index) = 0;
	virtual void deleteRule(int index) = 0;
	virtual bool isRulesValid() = 0;
};
