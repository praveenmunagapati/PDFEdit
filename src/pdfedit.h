#ifndef __PDFEDIT_H__
#define __PDFEDIT_H__
#include <qapplication.h>
#include <qpushbutton.h>
#include <qfont.h>
#include <qsplitter.h>
#include <qmainwindow.h>

/** pdfEditWidget - class handling main application window */
class pdfEditWidget : public QMainWindow {
 Q_OBJECT
public:
 pdfEditWidget(QWidget *parent = 0, const char *name = 0);
 void createNewWindow(); 
protected:
 void closeEvent(QCloseEvent *e);
public slots:
 void exitApp();
 void closeWindow();
 void menuActivated(int id);
};

void createNewEditorWindow();

#endif
