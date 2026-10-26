#pragma once

#include <shv/iotqt/rpc/clientconnection.h>

#include <QMap>
#include <QStandardItemModel>
#include <QSet>
#include <QStringList>

class RolesTreeModel : public QStandardItemModel
{
	Q_OBJECT
public:
	enum Role { NameRole = Qt::UserRole+1, CycleRole };

	RolesTreeModel(QObject *parent = nullptr);

	void load(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path);
	void load(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path, const QVector<std::string> &role_names);
	void clearRoleData();
	void setRoleHierarchy(const QString &role);
	QModelIndex indexForRole(const QString &role) const;

	QStringList selectedRoles();
	void setSelectedRoles(const QStringList &roles);

	Q_SIGNAL void loadFinished();
	Q_SIGNAL void loadError(QString error);

protected:
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
	void loadRoles(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path, QVector<std::string> not_loaded_roles);

private:
	enum class CyclePolicy { Skip, Mark };

	void checkPartialySubRoles();
	void generateTree();
	void generateSubTree(QStandardItem *parent_item, const QString &role, const QMap<QString, QStringList> &adjacent_roles, QSet<QString> roles_path, CyclePolicy cycle_policy);

	QSet<QString> allSubRoles();
	QSet<QString> flattenRole(QStandardItem *parent_item);

	QMap<QString, std::vector<std::string>> m_shvRoles;
};
