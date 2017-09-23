        final List<TimeSeriesCollection> tsdata = Stream.<TimeSeriesCollection>builder()
                .add(new SimpleTimeSeriesCollection(
                        DateTime.parse("1980-01-01T08:00:00.000Z"),
                        Stream.of(
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "histogram"),
                                                Tags.valueOf(singletonMap("true", MetricValue.TRUE))),
                                        singletonMap(
                                                MetricName.valueOf("hist", "o", "gram"),
                                                MetricValue.fromHistValue(
                                                        new Histogram(
                                                                new Histogram.RangeWithCount(0, 1, 2),
                                                                new Histogram.RangeWithCount(3, 4, 5))))),
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "int"),
                                                Tags.valueOf(singletonMap("false", MetricValue.FALSE))),
                                        singletonMap(
                                                MetricName.valueOf("i", "n", "t"),
                                                MetricValue.fromIntValue(42)))
                        )))
                .add(new SimpleTimeSeriesCollection(
                        DateTime.parse("1990-01-01T09:00:00.000Z"),
                        Stream.of(
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "histogram"),
                                                Tags.EMPTY),
                                        singletonMap(
                                                MetricName.valueOf("hist", "o", "gram"),
                                                MetricValue.fromHistValue(
                                                        new Histogram(
                                                                new Histogram.RangeWithCount(0, 1, 2),
                                                                new Histogram.RangeWithCount(3, 4, 5))))),
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "flt"),
                                                Tags.EMPTY),
                                        singletonMap(
                                                MetricName.valueOf("f", "l", "o", "a", "t"),
                                                MetricValue.fromDblValue(Math.E))),
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "empty"),
                                                Tags.EMPTY),
                                        singletonMap(
                                                MetricName.valueOf("value"),
                                                MetricValue.EMPTY)),
                                new ImmutableTimeSeriesValue(
                                        GroupName.valueOf(
                                                SimpleGroupPath.valueOf("test", "string"),
                                                Tags.EMPTY),
                                        ImmutableMap.<MetricName, MetricValue>builder()
                                        .put(
                                                MetricName.valueOf("value"),
                                                MetricValue.fromStrValue("a string"))
                                        .put(
                                                MetricName.valueOf("another"),
                                                MetricValue.fromStrValue("string"))
                                        .build())
                        )))
                .build()
                .collect(Collectors.toList());

        new FileSupport(new FileListFileSupport(), true)
                .create_file(new File("/tmp/tsdata_v2_list.tsd").toPath(), tsdata);

