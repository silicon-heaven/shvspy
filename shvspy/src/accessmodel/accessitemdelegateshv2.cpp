#include "accessitemdelegateshv2.h"
#include "accessmodelshv2.h"

#include <QLineEdit>
#include <QMessageBox>

AccessItemDelegateShv2::AccessItemDelegateShv2(QObject *parent)
	: QStyledItemDelegate(parent)
{

}

QWidget *AccessItemDelegateShv2::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);

	auto *editor = new QLineEdit(parent);
	return editor;
}

void AccessItemDelegateShv2::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	auto *e = qobject_cast<QLineEdit*>(editor);
	if (index.isValid() && e) {
		std::string val = e->text().trimmed().toStdString();

		if (index.column() == AccessModelShv2::Columns::ColAccess) {
			std::string err;
			shv::chainpack::RpcValue rv = shv::chainpack::RpcValue::fromCpon(val, &err);
			if (!err.empty()) {
				rv = shv::chainpack::RpcValue::fromCpon('"' + val + '"', &err);
			}
			if (err.empty()) {
				model->setData(index, qobject_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
			}
			else{
				QString msg =tr("In column") + " " + AccessModelShv2::columnName(index.column()) + " " + tr("is not valid chainpack.") + " " + tr("For example \"cmd\"");
				QMessageBox::critical(editor, tr("Invalid data"), msg);
			}
		}
		else if (index.column() == AccessModelShv2::Columns::ColPath) {
			if (!val.empty()){
				model->setData(index, qobject_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
			}
			else{
				QString msg =tr("Column") + " " + AccessModelShv2::columnName(index.column()) + " " + tr("is empty.");
				QMessageBox::critical(editor, tr("Invalid data"), msg);
			}
		}
		//else if (index.column() == AccessModel::Columns::ColMethod) {
		//	model->setData(index, qobject_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
		//}
		else {
			Super::setModelData(editor, model, index);
		}
	}
	else {
		Super::setModelData(editor, model, index);
	}
}
