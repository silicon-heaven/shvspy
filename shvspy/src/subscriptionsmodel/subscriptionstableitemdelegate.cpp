#include "subscriptionstableitemdelegate.h"

#include "subscriptionsmodel.h"

#include <QComboBox>
#include <shv/chainpack/rpcvalue.h>

SubscriptionsTableItemDelegate::SubscriptionsTableItemDelegate(QObject *parent):
	Super(parent)
{

}

QWidget *SubscriptionsTableItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	if (index.column() == SubscriptionsModel::Columns::ColMethod){
		auto *editor = new QComboBox(parent);
		editor->setEditable(true);
		editor->setInsertPolicy(QComboBox::NoInsert);
		editor->addItem(shv::chainpack::Rpc::SIG_VAL_CHANGED);
		editor->addItem(shv::chainpack::Rpc::SIG_MOUNTED_CHANGED);
		editor->addItem("fastchng");

		return editor;
	}

	return Super::createEditor(parent, option, index);
}

void SubscriptionsTableItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const
{
	if (index.column() == SubscriptionsModel::Columns::ColMethod){
		auto *cb = static_cast<QComboBox*>(editor);
		cb->setCurrentText(index.data(Qt::DisplayRole).toString());

		for(int i = 0; i < cb->count(); ++i){
			if (cb->itemData(i, Qt::DisplayRole) == index.data(Qt::DisplayRole)){
				cb->setCurrentIndex(i);
			}
		}
	}
	else{
		Super::setEditorData(editor, index);
	}
}

void SubscriptionsTableItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
	if (index.column() == SubscriptionsModel::Columns::ColMethod){
		auto *cb = static_cast<QComboBox*>(editor);
		model->setData(index, cb->currentText(), Qt::EditRole);
	}
	else{
		Super::setModelData(editor, model, index);
	}
}

void SubscriptionsTableItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(index);
	editor->setGeometry(option.rect);
}
