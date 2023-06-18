#ifndef SERVERTREEVIEW_H
#define SERVERTREEVIEW_H

#include <QTreeView>

class ServerTreeView : public QTreeView
{
	Q_OBJECT
private:
	typedef QTreeView Super;
public:
	ServerTreeView(QWidget *parent);
	Q_SIGNAL void enterKeyPressed(const QModelIndex &ix);
protected:
	void keyPressEvent(QKeyEvent *ev) Q_DECL_OVERRIDE;
};

#endif // SERVERTREEVIEW_H
