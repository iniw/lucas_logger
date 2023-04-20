#ifndef QLOGGER_H
#define QLOGGER_H

#include <QMainWindow>
#include <QSerialPort>
#ifdef ANDROID
#include <QJniEnvironment>
#endif

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
    void enviarComando(std::string_view cmd);
    void finalizar();
    void reiniciarInterface();

#ifdef ANDROID
    static void javaMensagemRecebida(JNIEnv* env, jobject thiz, jbyteArray str);
#endif

    QSerialPort* port;

private:
    Ui::QLogger *ui;
};
#endif // QLOGGER_H
