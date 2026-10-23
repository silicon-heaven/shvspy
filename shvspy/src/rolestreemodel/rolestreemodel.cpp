#include "rolestreemodel.h"

#include <shv/iotqt/rpc/rpccall.h>
#include <shv/core/log.h>

#include <shv/iotqt/acl/aclrole.h>

#include <QApplication>
#include <QBrush>
#include <QPalette>

static const std::string VALUE_METHOD = "value";

namespace {
void styleHierarchySection(QStandardItem *item)
{
	item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
	item->setBackground(QApplication::palette().brush(QPalette::Dark));
	item->setForeground(QApplication::palette().brush(QPalette::Light));
	QFont font = item->font();
	font.setBold(true);
	item->setFont(font);
}
}

RolesTreeModel::RolesTreeModel(QObject * parent)
	: QStandardItemModel(parent)
{
}

bool RolesTreeModel::setData(const QModelIndex & ix, const QVariant & val, int role)
{
	bool ret = true;

	if(role == Qt::CheckStateRole) {
		ret = QStandardItemModel::setData(ix, val, role);

		if (val.toInt() != Qt::PartiallyChecked) {
			checkPartialySubRoles();
		}
	}
	else {
		ret = QStandardItemModel::setData(ix, val, role);
	}
	return ret;
}

void RolesTreeModel::load(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path)
{
	if (rpc_connection == nullptr) {
		emit loadError(tr("Cannot load roles: RPC connection is unavailable."));
		return;
	}
	clearRoleData();

	int rqid = rpc_connection->nextRequestId();
	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(rpc_connection, rqid, this);

	cb->start(this, [this, rpc_connection, acl_etc_roles_node_path](const shv::chainpack::RpcResponse &response) {
		if(response.isValid()){
			if(response.isError()) {
				emit loadError(tr("Failed to load roles.") + " " + QString::fromStdString(response.error().toString()));
			}
			else{
				if (response.result().isList()){
					QVector<std::string> roles;
					const auto result = response.result();
					for (const auto &v : result.asList()){
						roles.push_back(v.asString());
					}
					load(rpc_connection, acl_etc_roles_node_path, roles);
				}
				else {
					emit loadError(tr("Invalid response while loading roles."));
				}
			}
		}
		else{
			emit loadError(tr("Request timeout expired for method: %1 path: %2").arg("ls", QString::fromStdString(acl_etc_roles_node_path)));
		}
	});
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	connect(this, &RolesTreeModel::loadFinished, this, &RolesTreeModel::generateTree, Qt::SingleShotConnection);
#else
	connect(this, &RolesTreeModel::loadFinished, this, &RolesTreeModel::generateTree);
	#endif
	rpc_connection->callShvMethod(rqid, acl_etc_roles_node_path, shv::chainpack::Rpc::METH_LS);
}

void RolesTreeModel::loadRoles(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path, QVector<std::string> not_loaded_roles)
{
	if (rpc_connection == nullptr) {
		return;
	}

	int rqid = rpc_connection->nextRequestId();
	std::string role_name = not_loaded_roles.takeFirst();

	auto *cb = new shv::iotqt::rpc::RpcResponseCallBack(rpc_connection, rqid, this);
	std::string role_path = acl_etc_roles_node_path + "/" + role_name;

	cb->start(this, [this, rpc_connection, acl_etc_roles_node_path, role_path, role_name, not_loaded_roles](const shv::chainpack::RpcResponse &response) {
		if (response.isValid()) {
			if (response.isError()) {
				emit loadError(tr("Failed to load role: %1").arg(QString::fromStdString(role_name)) + " " + QString::fromStdString(response.error().toString()));
			}
			else {
				if (response.result().isMap()){
					shv::iotqt::acl::AclRole r = shv::iotqt::acl::AclRole::fromRpcValue(response.result()).value();
					m_shvRoles[QString::fromStdString(role_name)] = r.roles;
				}
				if (not_loaded_roles.isEmpty()) {
					emit loadFinished();
				}
				else {
					loadRoles(rpc_connection, acl_etc_roles_node_path, not_loaded_roles);
				}
			}
		}
		else {
			emit loadError(tr("Request timeout expired for method: %1 path: %2").arg(QString::fromStdString(VALUE_METHOD), QString::fromStdString(role_path)));
		}
	});

	rpc_connection->callShvMethod(rqid, role_path, VALUE_METHOD);
}

void RolesTreeModel::load(shv::iotqt::rpc::ClientConnection *rpc_connection, const std::string &acl_etc_roles_node_path, const QVector<std::string> &role_names)
{
	if (rpc_connection == nullptr) {
		emit loadError(tr("Cannot load roles: RPC connection is unavailable."));
		return;
	}
	clearRoleData();
	loadRoles(rpc_connection, acl_etc_roles_node_path, role_names);
}

void RolesTreeModel::clearRoleData()
{
	m_shvRoles.clear();
	clear();
	setColumnCount(1);
	setHorizontalHeaderLabels({ QString() });
}

void RolesTreeModel::setSelectedRoles(const QStringList &roles)
{
	for (const auto &role : roles) {
		QStandardItem *root_item = invisibleRootItem();

		for (int i = 0; i < root_item->rowCount(); i++) {
			QStandardItem *it = root_item->child(i);
			if (role == it->data().toString()){
				it->setCheckState(Qt::Checked);
			}
		}
	}

	checkPartialySubRoles();
}

void RolesTreeModel::setRoleHierarchy(const QString &role)
{
	clear();
	setHorizontalHeaderLabels({ role });

	QMap<QString, QStringList> base_roles;
	QMap<QString, QStringList> derived_roles;
	for (auto it = m_shvRoles.cbegin(); it != m_shvRoles.cend(); ++it) {
		for (const auto &base_role : it.value()) {
			const QString base_role_name = QString::fromStdString(base_role);
			base_roles[it.key()].append(base_role_name);
			if (!derived_roles[base_role_name].contains(it.key())) {
				derived_roles[base_role_name].append(it.key());
			}
		}
	}

	const auto direct_base_roles = base_roles.value(role);
	QSet<QString> all_base_roles;
	QVector<QString> pending_base_roles{role};
	while (!pending_base_roles.isEmpty()) {
		const QString current_role = pending_base_roles.takeLast();
		for (const auto &base_role_name : base_roles.value(current_role)) {
			if (base_role_name != role && !all_base_roles.contains(base_role_name)) {
				all_base_roles.insert(base_role_name);
				pending_base_roles.push_back(base_role_name);
			}
		}
	}

	auto *bases_item = new QStandardItem(tr("Bases [%1]").arg(all_base_roles.count()));
	bases_item->setFlags(bases_item->flags() & ~Qt::ItemIsEditable);
	styleHierarchySection(bases_item);
	invisibleRootItem()->appendRow(bases_item);
	QSet<QString> roles_path{role};
	QSet<QString> direct_bases_seen;
	for (const auto &base_role : direct_base_roles) {
		if (!direct_bases_seen.contains(base_role)) {
			direct_bases_seen.insert(base_role);
			generateSubTree(bases_item, base_role, base_roles, roles_path, CyclePolicy::Mark);
		}
	}

	QSet<QString> all_derived_roles;
	QVector<QString> pending_derived_roles{role};
	while (!pending_derived_roles.isEmpty()) {
		const QString current_role = pending_derived_roles.takeLast();
		for (const auto &derived_role : derived_roles.value(current_role)) {
			if (derived_role != role && !all_derived_roles.contains(derived_role)) {
				all_derived_roles.insert(derived_role);
				pending_derived_roles.push_back(derived_role);
			}
		}
	}

	const auto direct_derived_roles = derived_roles.value(role);
	auto *derived_item = new QStandardItem(tr("Derived [%1]").arg(all_derived_roles.size()));
	derived_item->setFlags(derived_item->flags() & ~Qt::ItemIsEditable);
	styleHierarchySection(derived_item);
	invisibleRootItem()->appendRow(derived_item);
	for (const auto &derived_role : direct_derived_roles) {
		generateSubTree(derived_item, derived_role, derived_roles, roles_path, CyclePolicy::Mark);
	}

	QMap<QString, int> role_occurrences;
	QVector<QStandardItem *> pending_items;
	for (int row = 0; row < invisibleRootItem()->rowCount(); ++row) {
		pending_items.push_back(invisibleRootItem()->child(row));
	}
	while (!pending_items.isEmpty()) {
		QStandardItem *item = pending_items.takeLast();
		if (item->data(NameRole).isValid() && !item->data(CycleRole).toBool()) {
			++role_occurrences[item->data(NameRole).toString()];
		}
		for (int row = 0; row < item->rowCount(); ++row) {
			pending_items.push_back(item->child(row));
		}
	}

	for (int row = 0; row < invisibleRootItem()->rowCount(); ++row) {
		pending_items.push_back(invisibleRootItem()->child(row));
	}
	while (!pending_items.isEmpty()) {
		QStandardItem *item = pending_items.takeLast();
		const QString item_role = item->data(NameRole).toString();
		if (item->data(CycleRole).toBool()) {
			item->setForeground(QBrush(Qt::red));
			item->setToolTip(tr("Circular inheritance"));
		}
		else if (role_occurrences.value(item_role) > 1) {
			item->setForeground(QBrush(Qt::darkGreen));
			item->setToolTip(tr("This role appears in multiple hierarchy branches"));
		}
		for (int row = 0; row < item->rowCount(); ++row) {
			pending_items.push_back(item->child(row));
		}
	}
}

QModelIndex RolesTreeModel::indexForRole(const QString &role) const
{
	QVector<QModelIndex> pending_indexes;
	for (int row = 0; row < rowCount(); ++row) {
		pending_indexes.push_back(index(row, 0));
	}
	while (!pending_indexes.isEmpty()) {
		const QModelIndex current_index = pending_indexes.takeLast();
		if (current_index.data(NameRole).toString() == role) {
			return current_index;
		}
		for (int row = 0; row < rowCount(current_index); ++row) {
			pending_indexes.push_back(index(row, 0, current_index));
		}
	}
	return {};
}

QStringList RolesTreeModel::selectedRoles()
{
	QStringList roles;
	QStandardItem *root_item = invisibleRootItem();

	for(int i = 0; i < root_item->rowCount(); i++) {
		QStandardItem *it = root_item->child(i);

		if (it->checkState() == Qt::CheckState::Checked){
			roles << it->data().toString();
		}
	}
	return roles;
}

void RolesTreeModel::generateTree()
{
	clear();
	QStringList sl;
	sl << tr("Role");
	setHorizontalHeaderLabels(sl);

	QMap<QString, QStringList> adjacent_roles;
#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
	for (const auto &[role, sub_roles] : m_shvRoles.asKeyValueRange()) {
#else
	for (const auto &role: m_shvRoles.keys()){
		const auto &sub_roles = m_shvRoles.value(role);
#endif
		for (const auto &sub_role : sub_roles) {
			adjacent_roles[role].append(QString::fromStdString(sub_role));
		}
	}

	QStandardItem *parent_item = invisibleRootItem();

#if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
	for (const auto &[role_name, role_value] : m_shvRoles.asKeyValueRange()){
#else
	for (const auto &role_name: m_shvRoles.keys()){
		const auto& role_value = m_shvRoles.value(role_name);
#endif
		QList<QStandardItem *> row;
		auto *it = new QStandardItem(role_name);
		it->setData(role_name, NameRole);
		it->setCheckable(true);
		it->setFlags(it->flags() & ~Qt::ItemIsEditable);
		row << it;
		parent_item->appendRow(row);

		QSet<QString> roles_path{role_name};
		for (const auto &subrole : role_value) {
			generateSubTree(it, QString::fromStdString(subrole), adjacent_roles, roles_path, CyclePolicy::Skip);
		}
	}
}

void RolesTreeModel::generateSubTree(QStandardItem *parent_item, const QString &role, const QMap<QString, QStringList> &adjacent_roles, QSet<QString> roles_path, CyclePolicy cycle_policy)
{
	if (roles_path.contains(role)) {
		shvWarning() << "generate role tree cyclic reference";
		if (cycle_policy == CyclePolicy::Mark) {
			auto *cycle_item = new QStandardItem(role);
			cycle_item->setData(role, NameRole);
			cycle_item->setData(true, CycleRole);
			cycle_item->setFlags(cycle_item->flags() & ~Qt::ItemIsEditable);
			parent_item->appendRow(cycle_item);
		}
		return;
	}

	auto *it = new QStandardItem(role);
	it->setData(role, NameRole);
	it->setFlags(it->flags() & ~Qt::ItemIsEditable);
	parent_item->appendRow(it);

	roles_path.insert(role);
	for (const auto &adjacent_role : adjacent_roles.value(role)) {
		generateSubTree(it, adjacent_role, adjacent_roles, roles_path, cycle_policy);
	}
}

QSet<QString> RolesTreeModel::allSubRoles()
{
	QSet<QString> roles;
	QStandardItem *root_item = invisibleRootItem();

	for (int i = 0; i < root_item->rowCount(); i++) {
		QStandardItem *it = root_item->child(i);

		if (it->checkState() == Qt::CheckState::Checked){
			for(int ch = 0; ch < it->rowCount(); ch++) {
				QStandardItem *it1 = it->child(ch);
				roles += flattenRole(it1);
			}
		}
	}

	return roles;
}

QSet<QString> RolesTreeModel::flattenRole(QStandardItem *parent_item)
{
	QSet<QString> ret;

	ret.insert(parent_item->data().toString());

	for (int i = 0; i < parent_item->rowCount(); i++) {
		QStandardItem *it = parent_item->child(i);
		QString role_name = it->data().toString();

		if(!ret.contains(role_name)) {
			ret += flattenRole(it);
		}
	}

	return ret;
}

void RolesTreeModel::checkPartialySubRoles()
{
	QSet<QString> selected_sub_roles = allSubRoles();
	QStandardItem *root_item = invisibleRootItem();

	for(int i = 0; i < root_item->rowCount(); i++) {
		QStandardItem *it = root_item->child(i);

		if (it->checkState() == Qt::CheckState::PartiallyChecked){
			if (!selected_sub_roles.contains(it->data().toString())){
				it->setCheckState(Qt::CheckState::Unchecked);
			}
		}
		else{
			if (selected_sub_roles.contains(it->data().toString())){
				it->setCheckState(Qt::CheckState::PartiallyChecked);
			}
		}
	}
}
