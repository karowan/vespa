// Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.hosted.controller.proxy;

import com.google.inject.Inject;
import com.yahoo.config.provision.zone.ZoneId;
import com.yahoo.jdisc.http.HttpRequest.Method;
import com.yahoo.log.LogLevel;
import com.yahoo.vespa.athenz.identity.ServiceIdentityProvider;
import com.yahoo.vespa.athenz.tls.AthenzIdentityVerifier;
import com.yahoo.vespa.hosted.controller.api.integration.zone.ZoneRegistry;
import org.apache.http.Header;
import org.apache.http.client.config.RequestConfig;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpDelete;
import org.apache.http.client.methods.HttpGet;
import org.apache.http.client.methods.HttpPatch;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.client.methods.HttpPut;
import org.apache.http.client.methods.HttpRequestBase;
import org.apache.http.entity.InputStreamEntity;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClientBuilder;
import org.apache.http.util.EntityUtils;

import javax.net.ssl.HostnameVerifier;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;
import java.time.Duration;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.logging.Logger;

import static com.yahoo.yolean.Exceptions.uncheck;


/**
 * @author Haakon Dybdahl
 * @author bjorncs
 */
@SuppressWarnings("unused") // Injected
public class ConfigServerRestExecutorImpl implements ConfigServerRestExecutor {

    private static final Logger log = Logger.getLogger(ConfigServerRestExecutorImpl.class.getName());

    private static final Duration PROXY_REQUEST_TIMEOUT = Duration.ofSeconds(10);
    private static final Set<String> HEADERS_TO_COPY = Set.of("X-HTTP-Method-Override", "Content-Type");

    private final ZoneRegistry zoneRegistry;
    private final ServiceIdentityProvider sslContextProvider;

    @Inject
    public ConfigServerRestExecutorImpl(ZoneRegistry zoneRegistry, ServiceIdentityProvider sslContextProvider) {
        this.zoneRegistry = zoneRegistry;
        this.sslContextProvider = sslContextProvider;
    }

    @Override
    public ProxyResponse handle(ProxyRequest proxyRequest) throws ProxyException {
        HostnameVerifier hostnameVerifier = createHostnameVerifier(proxyRequest.getZoneId());
        List<URI> allServers = getConfigserverEndpoints(proxyRequest.getZoneId());

        StringBuilder errorBuilder = new StringBuilder();
        if (queueFirstServerIfDown(allServers, hostnameVerifier)) {
            errorBuilder.append("Change ordering due to failed ping.");
        }
        for (URI uri : allServers) {
            Optional<ProxyResponse> proxyResponse = proxyCall(uri, proxyRequest, hostnameVerifier, errorBuilder);
            if (proxyResponse.isPresent()) {
                return proxyResponse.get();
            }
        }
        // TODO Add logging, for now, experimental and we want to not add more noise.
        throw new ProxyException(ErrorResponse.internalServerError("Failed talking to config servers: "
                + errorBuilder.toString()));
    }

    private List<URI> getConfigserverEndpoints(ZoneId zoneId) {
        // TODO: Use config server VIP for all zones that have one
        // Make a local copy of the list as we want to manipulate it in case of ping problems.
        if (zoneId.region().value().startsWith("aws-") || zoneId.region().value().contains("-aws-")) {
            return List.of(zoneRegistry.getConfigServerVipUri(zoneId));
        } else {
            return new ArrayList<>(zoneRegistry.getConfigServerUris(zoneId));
        }
    }

    private Optional<ProxyResponse> proxyCall(
            URI uri, ProxyRequest proxyRequest, HostnameVerifier hostnameVerifier, StringBuilder errorBuilder)
            throws ProxyException {
        final HttpRequestBase requestBase = createHttpBaseRequest(
                proxyRequest.getMethod(), proxyRequest.createConfigServerRequestUri(uri), proxyRequest.getData());
        // Empty list of headers to copy for now, add headers when needed, or rewrite logic.
        copyHeaders(proxyRequest.getHeaders(), requestBase);

        RequestConfig config = RequestConfig.custom()
                .setConnectTimeout((int) PROXY_REQUEST_TIMEOUT.toMillis())
                .setConnectionRequestTimeout((int) PROXY_REQUEST_TIMEOUT.toMillis())
                .setSocketTimeout((int) PROXY_REQUEST_TIMEOUT.toMillis()).build();
        try (
                CloseableHttpClient client = createHttpClient(config, sslContextProvider, hostnameVerifier);
                CloseableHttpResponse response = client.execute(requestBase)
        ) {
            String content = getContent(response);
            int status = response.getStatusLine().getStatusCode();
            if (status / 100 == 5) {
                errorBuilder.append("Talking to server ").append(uri.getHost());
                errorBuilder.append(", got ").append(status).append(" ")
                        .append(content).append("\n");
                log.log(LogLevel.DEBUG, () -> String.format("Got response from %s with status code %d and content:\n %s",
                                                            uri.getHost(), status, content));
                return Optional.empty();
            }
            final Header contentHeader = response.getLastHeader("Content-Type");
            final String contentType;
            if (contentHeader != null && contentHeader.getValue() != null && ! contentHeader.getValue().isEmpty()) {
                contentType = contentHeader.getValue().replace("; charset=UTF-8","");
            } else {
                contentType = "application/json";
            }
            // Send response back
            return Optional.of(new ProxyResponse(proxyRequest, content, status, uri, contentType));
        } catch (Exception e) {
            errorBuilder.append("Talking to server ").append(uri.getHost());
            errorBuilder.append(" got exception ").append(e.getMessage());
            log.log(LogLevel.DEBUG, e, () -> "Got exception while sending request to " + uri.getHost());
            return Optional.empty();
        }
    }

    private static String getContent(CloseableHttpResponse response) {
        return Optional.ofNullable(response.getEntity())
                .map(entity -> uncheck(() -> EntityUtils.toString(entity)))
                .orElse("");
    }

    private static HttpRequestBase createHttpBaseRequest(Method method, URI uri, InputStream data) throws ProxyException {
        switch (method) {
            case GET:
                return new HttpGet(uri);
            case POST:
                HttpPost post = new HttpPost(uri);
                if (data != null) {
                    post.setEntity(new InputStreamEntity(data));
                }
                return post;
            case PUT:
                HttpPut put = new HttpPut(uri);
                if (data != null) {
                    put.setEntity(new InputStreamEntity(data));
                }
                return put;
            case DELETE:
                return new HttpDelete(uri);
            case PATCH:
                HttpPatch patch = new HttpPatch(uri);
                if (data != null) {
                    patch.setEntity(new InputStreamEntity(data));
                }
                return patch;
            default:
                throw new ProxyException(ErrorResponse.methodNotAllowed("Will not proxy such calls."));
        }
    }

    private static void copyHeaders(Map<String, List<String>> headers, HttpRequestBase toRequest) {
        for (Map.Entry<String, List<String>> headerEntry : headers.entrySet()) {
            if (HEADERS_TO_COPY.contains(headerEntry.getKey())) {
                for (String value : headerEntry.getValue()) {
                    toRequest.addHeader(headerEntry.getKey(), value);
                }
            }
        }
    }

    /**
     * During upgrade, one server can be down, this is normal. Therefor we do a quick ping on the first server,
     * if it is not responding, we try the other servers first. False positive/negatives are not critical,
     * but will increase latency to some extent.
     */
    private boolean queueFirstServerIfDown(List<URI> allServers, HostnameVerifier hostnameVerifier) {
        if (allServers.size() < 2) {
            return false;
        }
        URI uri = allServers.get(0);
        HttpGet httpget = new HttpGet(uri);

        int timeout = 500;
        RequestConfig config = RequestConfig.custom()
                .setConnectTimeout(timeout)
                .setConnectionRequestTimeout(timeout)
                .setSocketTimeout(timeout).build();
        try (
                CloseableHttpClient client = createHttpClient(config, sslContextProvider, hostnameVerifier);
                CloseableHttpResponse response = client.execute(httpget)
        ) {
            if (response.getStatusLine().getStatusCode() == 200) {
                return false;
            }

        } catch (IOException e) {
            // We ignore this, if server is restarting this might happen.
        }
        // Some error happened, move this server to the back. The other servers should be running.
        Collections.rotate(allServers, -1);
        return true;
    }

    private HostnameVerifier createHostnameVerifier(ZoneId zoneId) {
        return new AthenzIdentityVerifier(Set.of(zoneRegistry.getConfigServerHttpsIdentity(zoneId)));
    }

    private static CloseableHttpClient createHttpClient(RequestConfig config,
                                                        ServiceIdentityProvider sslContextProvider,
                                                        HostnameVerifier hostnameVerifier) {
        return HttpClientBuilder.create()
                .setUserAgent("config-server-proxy-client")
                .setSslcontext(sslContextProvider.getIdentitySslContext())
                .setSSLHostnameVerifier(hostnameVerifier)
                .setDefaultRequestConfig(config)
                .build();
    }

}
