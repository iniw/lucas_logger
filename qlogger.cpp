#include "qlogger.h"
#include "ui_qlogger.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <string_view>
#include <QBuffer>
#ifdef ANDROID
#include <QtCore/qjniobject.h>
#include <android/log.h>

#define  LOG_TAG    "Lucas"
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#endif

static QLogger* instance = nullptr;

QLogger::QLogger(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::QLogger)
{
    ui->setupUi(this);
    setWindowTitle("Lucas Logger");
    instance = this;
#ifdef ANDROID
    const JNINativeMethod methods[] = {
       "dataReceived", "([B)V", (void*)&QLogger::javaMensagemRecebida
    };
    QJniEnvironment env;
    env.registerNativeMethods("com/qtlucas/qtlog/LoggerListener", methods, 1);

    jboolean arg = false;
    QJniObject::callStaticMethod<void>(
        "com/qtlucas/qtlog/Logger",
        "create",
        "(Landroid/content/Context;Z)V",
        QNativeInterface::QAndroidApplication::context(),
        arg);
#else
    tentarInicializarPort();
#endif
}

void QLogger::linhaRecebida(QByteArray str) {
    qDebug() << "recebido: " << str;
    if (str.front() == '$') {
        auto view = QLatin1StringView(str.data() + 1, str.size() - 1);
        auto separador = view.indexOf(':');
        if (separador == -1 || separador == view.size() - 1)
            return;

        auto nome = view.sliced(0, separador);
        auto valor = view.sliced(separador + 1);

        auto child = instance->findChild<QLabel*>(nome.toString());
        if (child)
            QMetaObject::invokeMethod(child, "setText", Qt::AutoConnection  , Q_ARG(QString, valor.toString()));
    } else {
        QMetaObject::invokeMethod(instance->ui->logSerial, "appendPlainText", Qt::AutoConnection, Q_ARG(QString, str));
    }
}

void QLogger::mensagemRecebida() {
    if (!port || !port->isOpen())
        return;

    while (port->canReadLine()) {
        auto str = port->readLine();

        while (str.back() == '\n')
            str.chop(1);

        linhaRecebida(std::move(str));

    }
}

#ifdef ANDROID
void QLogger::javaMensagemRecebida(JNIEnv* env, jobject, jbyteArray jdata) {
    static QByteArray buffer;

    jsize jdataSize = env->GetArrayLength(jdata);
    if (jdataSize == 0)
        return;

    auto jelements = env->GetByteArrayElements(jdata, nullptr);

    buffer.append((const char*)jelements, jdataSize);

    env->ReleaseByteArrayElements(jdata, jelements, JNI_ABORT);

    qsizetype i = buffer.indexOf('\n');
    while (i != -1) {
        linhaRecebida(buffer.first(i));
        buffer.remove(0, i + 1);
        i = buffer.indexOf('\n');
    }
}
#endif

void QLogger::enviarReceitaPadrao() {
    auto numBytes = port->write("$R\n");
    if (numBytes == -1)
        qDebug() << "falha ao escrever - [" << port->errorString() << "]";
    else
        qDebug() << "enviados " << numBytes << " bytes";
}

void QLogger::tentarInicializarPort() {
    port = acharPortValido();
    if (!port)
        return;

    port->setBaudRate(115200);
    if (!port->open(QSerialPort::ReadWrite)) {
        ui->statusBar->showMessage("falha ao abrir serial para leitura");
    } else {
        ui->statusBar->showMessage(QString("%1 | %2").arg(port->portName(), QString::number(port->baudRate())));
        QObject::connect(port, &QSerialPort::readyRead, this, &QLogger::mensagemRecebida);
        QObject::connect(ui->enviarReceitaPadrao, &QPushButton::clicked, this, &QLogger::enviarReceitaPadrao);
    }

}

void QLogger::finalizar() {
    if (port && port->isOpen())
        port->close();
}

QSerialPort* QLogger::acharPortValido() {
    auto list = QSerialPortInfo::availablePorts();
    if (list.empty()) {
        ui->statusBar->showMessage("nenhum dispositivo conectado");
        return nullptr;
    }

    for (auto& portInfo : list) {
        if (portInfo.vendorIdentifier() == 1155) {
            return new QSerialPort(portInfo, this);
        }
    }

    ui->statusBar->showMessage("nenhum port é valido");
    return nullptr;
}


QLogger::~QLogger()
{
    delete ui;
    if (port)
        delete port;
}

