#include "task.h"

task::task() {

}

QString task::getDeadlineText() const
{
    QDateTime nowDate(QDateTime::currentDateTime());

    qint64 secs = nowDate.secsTo(deadline);

    int days = secs / 86400;
    int hours = (secs % 86400) / 3600;
    int mins = (secs % 3600) / 60;

    return QString("%1д %2ч %3м").arg(days).arg(hours).arg(mins);
}
