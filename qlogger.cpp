#include "qlogger.h"
#include "ui_qlogger.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtCore/qjniobject.h>
#include <string_view>
#include <android/log.h>

#define  LOG_TAG    "Lucas"
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)


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
       "dataReceived", "(Ljava/lang/String;)V", (void*)&QLogger::javaMensagemRecebida
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
    qDebug() << str;
    if (str.front() == '$') {
            auto view = QLatin1StringView(str.data() + 1, str.size() - 1);
            auto separador = view.indexOf(':');

            auto nome = view.sliced(0, separador);
            auto valor = view.sliced(separador + 1);

            auto child = instance->findChild<QLabel*>(nome.toString());
            if (child)
                child->setText(valor.toString());

            if (nome == "ativa") {
                instance->ui->infoFila->setTitle(QString("Fila de estações | Ativa = ").append(valor));
                return;
            }
            return;
    }
    instance->ui->logSerial->appendPlainText(str);
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

void QLogger::javaMensagemRecebida(JNIEnv* env, jobject thiz, jstring jstr) {
    static QByteArray buffer = "";
    jsize jstrSize = env->GetStringUTFLength(jstr);
    if (jstrSize == 0)
        return;

    auto jdata = env->GetStringUTFChars(jstr, nullptr);

    buffer.append(jdata, jstrSize);
    auto str = QLatin1StringView((const char*)jdata, jstrSize);
    LOGD("recebido: %s", str.data());

    env->ReleaseStringUTFChars(jstr, jdata);

    qsizetype i = -1;
    while ((i = buffer.indexOf('\n')) != -1) {
        LOGD("pre-corte: %s", buffer.data());
        LOGD("enviado: %s", buffer.first(i).data());
        linhaRecebida(buffer.first(i));
        buffer.remove(0, i + 1);
        LOGD("pos-corte: %s", buffer.data());
    }
}

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

