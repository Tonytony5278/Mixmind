import { createSlice, createAsyncThunk, PayloadAction } from '@reduxjs/toolkit';
import { RootState } from '../index';

interface AppState {
  initialized: boolean;
  loading: boolean;
  loadingMessage: string;
  error: string | null;
  version: string;
  buildInfo: {
    version: string;
    buildDate: string;
    buildType: string;
    compiler: string;
    platform: string;
    features: string[];
  } | null;
  serverUrls: {
    rest: string;
    websocket: string;
  };
  connectionStatus: {
    rest: 'connected' | 'disconnected' | 'connecting' | 'error';
    websocket: 'connected' | 'disconnected' | 'connecting' | 'error';
  };
  performance: {
    cpuUsage: number;
    memoryUsage: number;
    diskUsage: number;
    activeConnections: number;
    totalRequests: number;
    averageResponseTime: number;
  };
}

const initialState: AppState = {
  initialized: false,
  loading: true,
  loadingMessage: 'Initializing MixMind AI...',
  error: null,
  version: '1.0.0',
  buildInfo: null,
  serverUrls: {
    rest: 'http://localhost:8080',
    websocket: 'ws://localhost:8081',
  },
  connectionStatus: {
    rest: 'disconnected',
    websocket: 'disconnected',
  },
  performance: {
    cpuUsage: 0,
    memoryUsage: 0,
    diskUsage: 0,
    activeConnections: 0,
    totalRequests: 0,
    averageResponseTime: 0,
  },
};

// Async thunks
export const initializeApp = createAsyncThunk(
  'app/initialize',
  async (_, { dispatch }) => {
    try {
      // Check server connectivity
      const response = await fetch('/api/info');
      if (!response.ok) {
        throw new Error('Failed to connect to server');
      }
      
      const info = await response.json();
      
      // Get build information
      const buildResponse = await fetch('/api/build-info');
      let buildInfo = null;
      if (buildResponse.ok) {
        buildInfo = await buildResponse.json();
      }
      
      return {
        version: info.version || '1.0.0',
        buildInfo,
        serverUrls: {
          rest: info.url || 'http://localhost:8080',
          websocket: info.websocketUrl || 'ws://localhost:8081',
        },
      };
    } catch (error) {
      throw new Error(error instanceof Error ? error.message : 'Unknown error');
    }
  }
);

export const fetchPerformanceMetrics = createAsyncThunk(
  'app/fetchPerformanceMetrics',
  async () => {
    const response = await fetch('/api/stats');
    if (!response.ok) {
      throw new Error('Failed to fetch performance metrics');
    }
    return await response.json();
  }
);

export const runHealthCheck = createAsyncThunk(
  'app/runHealthCheck',
  async () => {
    const response = await fetch('/api/health');
    if (!response.ok) {
      throw new Error('Health check failed');
    }
    return await response.json();
  }
);

const appSlice = createSlice({
  name: 'app',
  initialState,
  reducers: {
    setLoadingMessage: (state, action: PayloadAction<string>) => {
      state.loadingMessage = action.payload;
    },
    clearError: (state) => {
      state.error = null;
    },
    setConnectionStatus: (state, action: PayloadAction<{
      service: 'rest' | 'websocket';
      status: 'connected' | 'disconnected' | 'connecting' | 'error';
    }>) => {
      state.connectionStatus[action.payload.service] = action.payload.status;
    },
    updatePerformanceMetrics: (state, action: PayloadAction<Partial<AppState['performance']>>) => {
      state.performance = { ...state.performance, ...action.payload };
    },
  },
  extraReducers: (builder) => {
    builder
      // Initialize app
      .addCase(initializeApp.pending, (state) => {
        state.loading = true;
        state.loadingMessage = 'Connecting to MixMind AI server...';
        state.error = null;
      })
      .addCase(initializeApp.fulfilled, (state, action) => {
        state.loading = false;
        state.initialized = true;
        state.version = action.payload.version;
        state.buildInfo = action.payload.buildInfo;
        state.serverUrls = action.payload.serverUrls;
        state.connectionStatus.rest = 'connected';
      })
      .addCase(initializeApp.rejected, (state, action) => {
        state.loading = false;
        state.error = action.error.message || 'Failed to initialize application';
        state.connectionStatus.rest = 'error';
      })
      // Fetch performance metrics
      .addCase(fetchPerformanceMetrics.fulfilled, (state, action) => {
        state.performance = {
          cpuUsage: action.payload.cpu_usage || 0,
          memoryUsage: action.payload.memory_usage || 0,
          diskUsage: action.payload.disk_usage || 0,
          activeConnections: action.payload.active_connections || 0,
          totalRequests: action.payload.total_requests || 0,
          averageResponseTime: action.payload.average_response_time_ms || 0,
        };
      })
      // Health check
      .addCase(runHealthCheck.fulfilled, (state, action) => {
        if (action.payload.status === 'healthy') {
          state.connectionStatus.rest = 'connected';
        }
      })
      .addCase(runHealthCheck.rejected, (state) => {
        state.connectionStatus.rest = 'error';
      });
  },
});

// Actions
export const { 
  setLoadingMessage, 
  clearError, 
  setConnectionStatus, 
  updatePerformanceMetrics 
} = appSlice.actions;

// Selectors
export const selectAppStatus = (state: RootState) => ({
  initialized: state.app.initialized,
  loading: state.app.loading,
  loadingMessage: state.app.loadingMessage,
  error: state.app.error,
});

export const selectAppVersion = (state: RootState) => state.app.version;
export const selectBuildInfo = (state: RootState) => state.app.buildInfo;
export const selectServerUrls = (state: RootState) => state.app.serverUrls;
export const selectConnectionStatus = (state: RootState) => state.app.connectionStatus;
export const selectPerformanceMetrics = (state: RootState) => state.app.performance;

export default appSlice.reducer;