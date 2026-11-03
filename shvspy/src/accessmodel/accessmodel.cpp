#include "accessmodel.h"

AccessModel::AccessModel(QObject *parent)
	: Super(parent)
{
}

void AccessModel::moveRuleToTop(int index)
{
	if (index < 0 || index >= rowCount()) {
		return;
	}

	while (index > 0) {
		moveRuleUp(index);
		--index;
	}
}

void AccessModel::moveRuleToBottom(int index)
{
	if (index < 0 || index >= rowCount()) {
		return;
	}

	while (index < rowCount() - 1) {
		moveRuleDown(index);
		++index;
	}
}
