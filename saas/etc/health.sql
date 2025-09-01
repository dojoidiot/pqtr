-- Health Check and Monitoring Functions
-- Add these to your database for production monitoring

-- Create a health check function
CREATE OR REPLACE FUNCTION health_check()
RETURNS JSON AS $$
DECLARE
    result JSON;
    db_status TEXT;
    table_counts JSON;
    uptime INTERVAL;
BEGIN
    -- Check database status
    SELECT 'healthy' INTO db_status;
    
    -- Get table row counts
    SELECT json_object_agg(table_name, row_count) INTO table_counts
    FROM (
        SELECT 
            schemaname || '.' || tablename as table_name,
            n_tup_ins + n_tup_upd + n_tup_del as row_count
        FROM pg_stat_user_tables
        WHERE schemaname = 'public'
    ) t;
    
    -- Get database uptime
    SELECT now() - pg_postmaster_start_time() INTO uptime;
    
    -- Build result
    result := json_build_object(
        'status', 'healthy',
        'timestamp', now(),
        'database', db_status,
        'uptime', uptime,
        'table_counts', table_counts,
        'version', '1.0.0'
    );
    
    RETURN result;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Create a simple ping function
CREATE OR REPLACE FUNCTION ping()
RETURNS TEXT AS $$
BEGIN
    RETURN 'pong';
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Grant access to anon role for health checks
GRANT EXECUTE ON FUNCTION health_check() TO anon;
GRANT EXECUTE ON FUNCTION ping() TO anon;

-- Create a performance monitoring function
CREATE OR REPLACE FUNCTION performance_metrics()
RETURNS JSON AS $$
DECLARE
    result JSON;
    active_connections INTEGER;
    cache_hit_ratio NUMERIC;
    slow_queries INTEGER;
BEGIN
    -- Get active connections
    SELECT count(*) INTO active_connections
    FROM pg_stat_activity
    WHERE state = 'active';
    
    -- Get cache hit ratio
    SELECT 
        round(
            (sum(heap_blks_hit) * 100.0 / (sum(heap_blks_hit) + sum(heap_blks_read)))::numeric, 2
        ) INTO cache_hit_ratio
    FROM pg_statio_user_tables;
    
    -- Get slow queries (queries taking more than 1 second)
    SELECT count(*) INTO slow_queries
    FROM pg_stat_activity
    WHERE state = 'active' 
    AND query_start < now() - interval '1 second';
    
    -- Build result
    result := json_build_object(
        'active_connections', active_connections,
        'cache_hit_ratio', cache_hit_ratio,
        'slow_queries', slow_queries,
        'timestamp', now()
    );
    
    RETURN result;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Grant access to authenticated users for performance metrics
GRANT EXECUTE ON FUNCTION performance_metrics() TO authenticated;

-- Create a table for storing health check history
CREATE TABLE IF NOT EXISTS health_check_history (
    id SERIAL PRIMARY KEY,
    check_time TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    check_type TEXT NOT NULL,
    status TEXT NOT NULL,
    details JSONB,
    response_time_ms INTEGER
);

-- Create index for performance
CREATE INDEX IF NOT EXISTS idx_health_check_history_time 
ON health_check_history(check_time);

-- Function to log health checks
CREATE OR REPLACE FUNCTION log_health_check(
    p_check_type TEXT,
    p_status TEXT,
    p_details JSONB DEFAULT NULL,
    p_response_time_ms INTEGER DEFAULT NULL
)
RETURNS VOID AS $$
BEGIN
    INSERT INTO health_check_history (
        check_type, 
        status, 
        details, 
        response_time_ms
    ) VALUES (
        p_check_type,
        p_status,
        p_details,
        p_response_time_ms
    );
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Grant access to anon role for logging
GRANT EXECUTE ON FUNCTION log_health_check(TEXT, TEXT, JSONB, INTEGER) TO anon;
GRANT INSERT ON health_check_history TO anon;

-- Create a cleanup function for old health check records
CREATE OR REPLACE FUNCTION cleanup_health_history(days_to_keep INTEGER DEFAULT 30)
RETURNS INTEGER AS $$
DECLARE
    deleted_count INTEGER;
BEGIN
    DELETE FROM health_check_history 
    WHERE check_time < now() - (days_to_keep || ' days')::INTERVAL;
    
    GET DIAGNOSTICS deleted_count = ROW_COUNT;
    
    RETURN deleted_count;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- Grant access to authenticated users for cleanup
GRANT EXECUTE ON FUNCTION cleanup_health_history(INTEGER) TO authenticated;

-- Create a view for recent health checks
CREATE OR REPLACE VIEW recent_health_checks AS
SELECT 
    check_time,
    check_type,
    status,
    response_time_ms,
    details
FROM health_check_history
WHERE check_time > now() - interval '24 hours'
ORDER BY check_time DESC;

-- Grant access to view
GRANT SELECT ON recent_health_checks TO anon;

-- Insert initial health check
SELECT log_health_check('initial_setup', 'success', '{"message": "Health monitoring initialized"}');
