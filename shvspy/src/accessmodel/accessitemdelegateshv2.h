#pragma once

#include <QStyledItemDelegate>

class AccessItemDelegateShv2 : public QStyledItemDelegate
{
	Q_OBJECT
private:
	using Super = QStyledItemDelegate;
public:
	AccessItemDelegateShv2(QObject *parent = nullptr);

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const Q_DECL_OVERRIDE;
};

