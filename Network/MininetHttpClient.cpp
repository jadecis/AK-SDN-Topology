#include "MininetHttpClient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

MininetHttpClient::MininetHttpClient(QObject *parent) :
    QObject(parent),
    m_baseUrl("http://127.0.0.1:5555"),
    m_nam(new QNetworkAccessManager(this))
{
    connect(m_nam, &QNetworkAccessManager::finished,
            this, &MininetHttpClient::onReplyFinished);
}

void MininetHttpClient::setBaseUrl(const QString &baseUrl)
{
    m_baseUrl = baseUrl;
}

QString MininetHttpClient::baseUrl() const
{
    return m_baseUrl;
}

void MininetHttpClient::send(const QString &endpoint, const QString &tag)
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::User, tag);
    /* Внутренний таймаут запроса 5 секунд (Qt 5.15+). */
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(5000);
#endif
    m_nam->get(req);
}

void MininetHttpClient::post(const QString &endpoint, const QByteArray &json, const QString &tag)
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("Content-Type", "application/json");
    req.setAttribute(QNetworkRequest::User, tag);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    req.setTransferTimeout(5000);
#endif
    m_nam->post(req, json);
}

void MininetHttpClient::checkHealth()
{
    send("/health", "health");
}

void MininetHttpClient::pingAll()
{
    send("/pingall", "pingall");
}

void MininetHttpClient::pingHosts(const QString &h1, const QString &h2)
{
    send(QString("/ping/%1/%2").arg(h1).arg(h2), QString("ping:%1:%2").arg(h1).arg(h2));
}

void MininetHttpClient::iperf(const QString &h1, const QString &h2)
{
    send(QString("/iperf/%1/%2").arg(h1).arg(h2), QString("iperf:%1:%2").arg(h1).arg(h2));
}

void MininetHttpClient::fetchHosts()
{
    send("/hosts", "hosts");
}

void MininetHttpClient::openXterm(const QString &host)
{
    send(QString("/xterm/%1").arg(host), QString("xterm:%1").arg(host));
}

void MininetHttpClient::shutdown()
{
    send("/shutdown", "shutdown");
}

void MininetHttpClient::pollTrafficEvents(double sinceTs)
{
    QString ep = QString("/traffic_events?since=%1").arg(sinceTs, 0, 'f', 6);
    send(ep, "traffic_events");
}

void MininetHttpClient::setLinkStatus(const QString &a, const QString &b, bool up)
{
    QJsonObject body;
    body["from"] = a;
    body["to"] = b;
    QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QString ep = up ? "/link/up" : "/link/down";
    QString tag = QString("link:%1:%2:%3").arg(a, b, up ? "up" : "down");
    post(ep, json, tag);
}

void MininetHttpClient::fetchDockerHealth()
{
    send("/docker/health", "docker_health");
}

static QVariantMap pingAllToVariantMap(const QJsonDocument &doc, int *successOut, int *totalOut)
{
    QVariantMap result;
    QVariantList matrix;
    int success = 0, total = 0;

    QJsonObject obj = doc.object();
    QJsonArray arr = obj.value("matrix").toArray();
    for (const QJsonValue &val : arr)
    {
        QJsonObject entry = val.toObject();
        QVariantMap m;
        m["src"] = entry.value("src").toString();
        m["dst"] = entry.value("dst").toString();
        m["rtt_ms"] = entry.value("rtt_ms").toDouble();
        m["loss"] = entry.value("loss").toDouble();
        matrix << m;
        total++;
        if (entry.value("loss").toDouble() < 100.0)
        {
            success++;
        }
    }
    result["matrix"] = matrix;
    result["success"] = success;
    result["total"] = total;
    if (successOut) *successOut = success;
    if (totalOut) *totalOut = total;
    return result;
}

void MininetHttpClient::onReplyFinished(QNetworkReply *reply)
{
    QString tag = reply->request().attribute(QNetworkRequest::User).toString();
    if (reply->error() != QNetworkReply::NoError)
    {
        emit requestFailed(tag, reply->errorString());
        reply->deleteLater();
        return;
    }

    QByteArray body = reply->readAll();
    reply->deleteLater();

    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(body, &perr);
    if (perr.error != QJsonParseError::NoError)
    {
        emit requestFailed(tag, QString("JSON parse error: %1").arg(perr.errorString()));
        return;
    }

    if (tag == "health")
    {
        bool ok = doc.object().value("ok").toBool(false);
        emit healthResult(ok);
        return;
    }

    if (tag == "pingall")
    {
        int success = 0, total = 0;
        QVariantMap m = pingAllToVariantMap(doc, &success, &total);
        emit pingAllResult(m);
        return;
    }

    if (tag.startsWith("ping:"))
    {
        QStringList parts = tag.split(':');
        QString h1 = parts.value(1);
        QString h2 = parts.value(2);
        QJsonObject obj = doc.object();
        double rtt = obj.value("rtt_ms").toDouble();
        double loss = obj.value("loss").toDouble();
        QString raw = obj.value("raw").toString();
        emit pingResult(h1, h2, rtt, loss, raw);
        return;
    }

    if (tag.startsWith("iperf:"))
    {
        QStringList parts = tag.split(':');
        QString h1 = parts.value(1);
        QString h2 = parts.value(2);
        double bw = doc.object().value("bw_mbps").toDouble();
        emit iperfResult(h1, h2, bw);
        return;
    }

    if (tag == "hosts")
    {
        QVariantMap hosts = doc.object().toVariantMap();
        emit hostsResult(hosts);
        return;
    }

    if (tag == "traffic_events")
    {
        QJsonArray arr = doc.object().value("events").toArray();
        QVariantList list;
        for (const QJsonValue &v : arr) list << v.toObject().toVariantMap();
        emit trafficEventsResult(list);
        return;
    }

    if (tag.startsWith("link:"))
    {
        QStringList parts = tag.split(':');
        QString a = parts.value(1);
        QString b = parts.value(2);
        bool up = parts.value(3) == "up";
        bool ok = doc.object().value("ok").toBool(false);
        emit linkStatusResult(a, b, up, ok);
        return;
    }

    if (tag == "docker_health")
    {
        QJsonArray arr = doc.object().value("containers").toArray();
        QVariantList list;
        for (const QJsonValue &v : arr) list << v.toObject().toVariantMap();
        emit dockerHealthResult(list);
        return;
    }
}
