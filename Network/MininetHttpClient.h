#ifndef MININETHTTPCLIENT_H
#define MININETHTTPCLIENT_H

#include <QObject>
#include <QString>
#include <QVariantMap>

class QNetworkAccessManager;
class QNetworkReply;

/* MininetHttpClient — HTTP-клиент для общения с мини-HTTP-сервером,
 * который встраивается в сгенерированный Mininet-скрипт (см.
 * MininetScriptBuilder). Сервер слушает 127.0.0.1:<port>.
 *
 * Доступные эндпойнты (см. шаблон в MininetScriptBuilder):
 *   GET /pingall                — выполнить net.pingAll()
 *   GET /ping/<h1>/<h2>          — h1.cmd("ping -c 4 ip_of_h2")
 *   GET /iperf/<h1>/<h2>         — net.iperf((h1, h2))
 *   GET /hosts                   — список хостов с IP
 *   GET /health                  — sanity-проверка, всегда {"ok": true}
 */
class MininetHttpClient : public QObject
{
    Q_OBJECT
public:
    explicit MininetHttpClient(QObject *parent = nullptr);

    void setBaseUrl(const QString &baseUrl);   // e.g. "http://127.0.0.1:5555"
    QString baseUrl() const;

    void checkHealth();
    void pingAll();
    void pingHosts(const QString &h1, const QString &h2);
    void iperf(const QString &h1, const QString &h2);
    void fetchHosts();
    void openXterm(const QString &host);
    void shutdown();
    /* Опрос накопленных событий трафика (с момента since unix-ts).
     * Эмитит trafficEventsResult. */
    void pollTrafficEvents(double sinceTs);

    /* === Симуляция отказа канала (#1) ===
     * POST /link/down или /link/up с телом {"from": a, "to": b}. */
    void setLinkStatus(const QString &a, const QString &b, bool up);

    /* === Health Docker-контейнеров (#3) ===
     * GET /docker/health → массив контейнеров с состоянием. */
    void fetchDockerHealth();

signals:
    /* health() — true если сервер ответил { "ok": true }. */
    void healthResult(bool ok);

    /* pingall — отдаёт распарсенный результат как QVariantMap:
     *   { "matrix": [{ "src": "h1", "dst": "h2", "rtt_ms": 0.5, "loss": 0 }, ...],
     *     "success": N, "total": M } */
    void pingAllResult(const QVariantMap &result);

    /* ping h1->h2 */
    void pingResult(const QString &h1, const QString &h2,
                    double rttMs, double lossPercent, const QString &raw);

    /* iperf h1->h2, bandwidth in Mbps */
    void iperfResult(const QString &h1, const QString &h2, double bwMbps);

    /* список хостов: { "h1": "10.0.0.1", "h2": "10.0.0.2", ... } */
    void hostsResult(const QVariantMap &hostsIp);

    /* События трафика — каждый элемент: { ts, src, dst, kind }. */
    void trafficEventsResult(QVariantList events);

    /* Результат смены состояния канала (#1). */
    void linkStatusResult(const QString &a, const QString &b, bool up, bool ok);

    /* Health Docker (#3): список контейнеров
     * [{ name, image, status, healthy, port_ok }]. */
    void dockerHealthResult(const QVariantList &containers);

    /* Любая ошибка запроса (HTTP, timeout, connection refused). */
    void requestFailed(const QString &endpoint, const QString &reason);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QString m_baseUrl;
    QNetworkAccessManager *m_nam;
    void send(const QString &endpoint, const QString &tag);
    void post(const QString &endpoint, const QByteArray &json, const QString &tag);
};

#endif // MININETHTTPCLIENT_H
