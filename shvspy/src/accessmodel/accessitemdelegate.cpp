#include "accessitemdelegate.h"
#include "accessmodel.h"

#include <QLineEdit>
#include <QMessageBox>

AccessItemDelegate::AccessItemDelegate(QObject *parent)
	: QStyledItemDelegate(parent)
{

}

QWidget *AccessItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);

	auto *editor = new QLineEdit(parent);
	return editor;
}

void AccessItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	auto *e = qobject_cast<QLineEdit*>(editor);
	if (index.isValid() && e) {
		std::string val = e->text().trimmed().toStdString();

                if (index.column() == AccessModel::Columns::ColAccess) {
			std::string err;
			shv::chainpack::RpcValue rv = shv::chainpack::RpcValue::fromCpon(val, &err);
			if (!err.empty()) {
				rv = shv::chainpack::RpcValue::fromCpon('"' + val + '"', &err);
			}
			if (err.empty()) {
				model->setData(index, qobject_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
			}
			else{
				QString msg =tr("In column") + " " + AccessModel::columnName(index.column()) + " " + tr("is not valid chainpack.") + " " + tr("For example \"cmd\"");
				QMessageBox::critical(editor, tr("Invalid data"), msg);
			}
		}
		else if (index.column() == AccessModel::Columns::ColPath) {
			if (!val.empty()){
				model->setData(index, qobject_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
			}
			else{
				QString msg =tr("Column") + " " + AccessModel::columnName(index.column()) + " " + tr("is empty.");
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
