#ifndef QLOGGER_H
#define QLOGGER_H

#include <QMainWindow>
#include <QSerialPort>
#include <QJniEnvironment>

QT_BEGIN_NAMESPACE
namespace Ui { class QLogger; }
QT_END_NAMESPACE

class QLogger : public QMainWindow
{
    Q_OBJECT

public:
    QLogger(QWidget *parent = nullptr);
    ~QLogger();

private:
    QSerialPort* acharPortValido();
    void tentarInicializarPort();

    static void linhaRecebida(QByteArray);

    void mensagemRecebida();
    void enviarReceitaPadrao();

    void finalizar();
    void reiniciarInterface();

    static void javaMensagemRecebida(JNIEnv* env, jobject thiz, jstring str);
    QSerialPort* port;

private:
    Ui::QLogger *ui;
};
#endif // QLOGGER_H
