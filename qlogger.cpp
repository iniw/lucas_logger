#include "qlogger.h"
#include "ui_qlogger.h"
#include <QSerialPort>
#include <QSerialPortInfo>
#include <string_view>
#include <algorithm>
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
    QObject::connect(ui->enviarCmdReceitaPadrao, &QPushButton::clicked, this, [this]() {
        instance->enviarBufferParaPlaca("L4", true);
    });
    QObject::connect(ui->enviarCmdHoming, &QPushButton::clicked, this, [this]() {
        instance->enviarBufferParaPlaca("G28 XY", true);
    });
    QObject::connect(ui->enviarGcode, &QPushButton::clicked, this, [this]() {
        auto bufferEscrito = ui->campoGcode->toPlainText().toLatin1();
        if (bufferEscrito.isEmpty())
            return;

        // se nada foi colocado na fila nós enviamos a primeira linha dos comandos que o usuario escreveu
        // e aguardamos receber o "ok" da placa para enviar a proxima
        if (s_filaParaEnviar.isEmpty())
            instance->enviarLinhaParaPlaca(bufferEscrito);

        s_filaParaEnviar.append(bufferEscrito);
    });
}

void QLogger::linhaRecebida(QByteArray bytes) {
    qDebug() << "recebido: " << bytes;
    if (bytes.front() == '$') {
        auto str = QLatin1StringView(bytes.data() + 1, bytes.size() - 1);
        auto separador = str.indexOf(':');
        if (separador == -1 || separador == str.size() - 1)
            return;

        auto nome = str.sliced(0, separador);
        auto valor = str.sliced(separador + 1);

        auto child = instance->findChild<QLabel*>(nome.toString());
        if (child)
            QMetaObject::invokeMethod(child, "setText", Qt::AutoConnection, Q_ARG(QString, valor.toString()));
    } else if (bytes == "ok") { // receber um "ok" da placa significa que um g-code vindo do serial USB acabou de terminar de executar
        if (s_filaParaEnviar.isEmpty())
            return;

        // ou seja, já podemos enviar a próxima linha
        instance->enviarLinhaParaPlaca(s_filaParaEnviar);
    } else {
        QMetaObject::invokeMethod(instance->ui->logSerial, "appendPlainText", Qt::AutoConnection, Q_ARG(QString, bytes));
    }
}

void QLogger::enviarLinhaParaPlaca(QByteArray& buffer) {
    // TODO: esses valores tem que vir da placa™
    constexpr qsizetype MAX_BYTES = 64;
    constexpr auto DELIMITADORES = std::array<char, 3>{'#', '%', '$'};

    static bool enviandoEspecial = false;
    static char delimitador = 0;
    auto it = std::find(DELIMITADORES.begin(), DELIMITADORES.end(), buffer.front());
    if (it != DELIMITADORES.end()) {
        delimitador = *it;
        qDebug() << "comecando envio especial do delimitador" << delimitador;
    }

    if (delimitador || enviandoEspecial) {
        auto proximoDelimitador = buffer.indexOf(delimitador, !enviandoEspecial);
        enviandoEspecial = true;
        if (proximoDelimitador == -1) {
            buffer.append(delimitador);
            proximoDelimitador = buffer.size() - 1;
        }

        auto bytes = std::min(proximoDelimitador + 1, MAX_BYTES);
        qDebug() << "enviando" << bytes << "bytes especiais";
        instance->enviarBufferParaPlaca(buffer.first(bytes), false);
        buffer.remove(0, bytes);

        if (bytes == proximoDelimitador + 1) {
            enviandoEspecial = false;
            delimitador = 0;
        }
    } else {
        auto i = buffer.indexOf('\n');
        qDebug() << "enviando uma linha de gcode";
        if (i == -1) {
            // se o buffer é composto por somente uma linha é consumido por completo
            instance->enviarBufferParaPlaca(buffer, true);
            buffer.clear();
        } else {
            // caso contrario consumimos somente a primeira linha
            instance->enviarBufferParaPlaca(buffer.first(i ? i : 1), true);
            buffer.remove(0, i + 1);
        }
    }
}


void QLogger::mensagemRecebida() {
    if (!port || !port->isOpen())
        return;

    while (port->canReadLine()) {
        auto bytes = port->readLine();
        while (bytes.back() == '\n')
            bytes.chop(1);

        linhaRecebida(std::move(bytes));
    }
}

#ifdef ANDROID
void QLogger::javaMensagemRecebida(JNIEnv* env, jobject, jbyteArray jdata) {
    static QByteArray buffer = {};

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

void QLogger::enviarBufferParaPlaca(QByteArray bytes, bool adicionarNewline) {
    if (bytes.isEmpty())
        return;

#ifdef ANDROID
    QJniEnvironment env;
    jbyteArray buffer = env->NewByteArray(cmd.size() + adicionarNewline);
    if (!buffer)
        return;

    env->SetByteArrayRegion(buffer, 0, cmd.size(), (const jbyte*)cmd.data());
    if (adicionarNewline)
        env->SetByteArrayRegion(buffer, cmd.size(), 1, (const jbyte*)"\n");

    QJniObject::callStaticMethod<void>(
        "com/qtlucas/qtlog/Logger",
        "enviar",
        "([B)V",
        buffer);

    env->DeleteLocalRef(buffer);
#else
    if (adicionarNewline)
        bytes.append('\n');

    auto numBytesEnviados = port->write(bytes);
    if (numBytesEnviados == -1)
        qDebug() << "falha ao enviar - [" << port->errorString() << "]";
    else if (numBytesEnviados != bytes.size())
        qDebug() << "não foi possivel enviar todos os bytes, sobraram" << bytes.size() - numBytesEnviados << "bytes";
    else
        qDebug() << "enviados" << bytes.size() << "bytes com sucesso\n" << bytes;
#endif
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
    }
}

void QLogger::finalizar()   {
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

