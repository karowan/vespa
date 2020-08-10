package com.yahoo.vespa.config.server.http.v2;

import com.yahoo.config.provision.ApplicationId;
import com.yahoo.container.jdisc.HttpResponse;
import com.yahoo.restapi.SlimeJsonResponse;
import com.yahoo.slime.Cursor;
import com.yahoo.slime.JsonFormat;
import com.yahoo.slime.Slime;
import com.yahoo.vespa.config.server.http.HttpConfigResponse;
import com.yahoo.vespa.config.server.metrics.ProtonMetricsAggregator;
import java.io.IOException;
import java.io.OutputStream;
import java.util.Map;

public class ProtonMetricsResponse extends SlimeJsonResponse {


    /**
     * @author akvalsvik
     */
    public ProtonMetricsResponse(int status, ApplicationId applicationId, Map<String, ProtonMetricsAggregator> aggregatedProtonMetrics) {
        super(status, buildSlime(applicationId, aggregatedProtonMetrics));
    }

    private static Slime buildSlime(ApplicationId applicationId, Map<String, ProtonMetricsAggregator> aggregatedProtonMetrics) {
        Slime slime = new Slime();
        Cursor application = slime.setObject();
        application.setString("applicationId", applicationId.serializedForm());

        Cursor clusters = application.setArray("clusters");

        for (var entry : aggregatedProtonMetrics.entrySet()) {
            Cursor cluster = clusters.addObject();
            cluster.setString("clusterId", entry.getKey());

            ProtonMetricsAggregator aggregator = entry.getValue();
            Cursor metrics = cluster.setObject("metrics");
            metrics.setDouble("documentsActiveCount", aggregator.aggregateDocumentActiveCount());
            metrics.setDouble("documentsReadyCount", aggregator.aggregateDocumentReadyCount());
            metrics.setDouble("documentsTotalCount", aggregator.aggregateDocumentTotalCount());
            metrics.setDouble("documentDiskUsage", aggregator.aggregateDocumentDiskUsage());
            metrics.setDouble("resourceDiskUsageAverage", aggregator.aggregateResourceDiskUsageAverage());
            metrics.setDouble("resourceMemoryUsageAverage", aggregator.aggregateResourceMemoryUsageAverage());
        }
        return slime;
    }
}
