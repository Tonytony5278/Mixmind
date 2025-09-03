import { createSlice, createAsyncThunk, PayloadAction } from '@reduxjs/toolkit';
import { RootState } from '../index';

interface WebSocketMessage {
  type: string;
  messageId: string;
  payload: any;
  timestamp: number;
}

interface WebSocketState {
  connected: boolean;
  connecting: boolean;
  error: string | null;
  connection: WebSocket | null;
  subscriptions: Set<string>;
  messageHistory: WebSocketMessage[];
  lastPingTime: number | null;
  latency: number;
  reconnectAttempts: number;
  maxReconnectAttempts: number;
}

const initialState: WebSocketState = {
  connected: false,
  connecting: false,
  error: null,
  connection: null,
  subscriptions: new Set(),
  messageHistory: [],
  lastPingTime: null,
  latency: 0,
  reconnectAttempts: 0,
  maxReconnectAttempts: 5,
};

// Async thunks
export const initializeWebSocket = createAsyncThunk(
  'websocket/initialize',
  async (_, { dispatch, getState }) => {
    const state = getState() as RootState;
    const wsUrl = state.app.serverUrls.websocket;
    
    return new Promise<WebSocket>((resolve, reject) => {
      try {
        const ws = new WebSocket(wsUrl);
        
        ws.onopen = () => {
          dispatch(connected(ws));
          resolve(ws);
        };
        
        ws.onclose = (event) => {
          dispatch(disconnected({ code: event.code, reason: event.reason }));
          
          // Auto-reconnect logic
          const currentState = (getState() as RootState).websocket;
          if (currentState.reconnectAttempts < currentState.maxReconnectAttempts) {
            setTimeout(() => {
              dispatch(reconnect());
            }, Math.pow(2, currentState.reconnectAttempts) * 1000);
          }
        };
        
        ws.onerror = (error) => {
          dispatch(connectionError('WebSocket connection failed'));
          reject(error);
        };
        
        ws.onmessage = (event) => {
          try {
            const message = JSON.parse(event.data);
            dispatch(messageReceived(message));
            
            // Handle pong messages for latency calculation
            if (message.type === 'pong' && message.payload.messageId) {
              const pingTime = currentState.lastPingTime;
              if (pingTime) {
                const latency = Date.now() - pingTime;
                dispatch(updateLatency(latency));
              }
            }
          } catch (error) {
            console.error('Failed to parse WebSocket message:', error);
          }
        };
        
      } catch (error) {
        reject(error);
      }
    });
  }
);

export const reconnect = createAsyncThunk(
  'websocket/reconnect',
  async (_, { dispatch, getState }) => {
    const state = (getState() as RootState).websocket;
    
    if (state.reconnectAttempts >= state.maxReconnectAttempts) {
      throw new Error('Maximum reconnection attempts exceeded');
    }
    
    dispatch(incrementReconnectAttempts());
    return dispatch(initializeWebSocket());
  }
);

export const sendMessage = createAsyncThunk(
  'websocket/sendMessage',
  async (message: Omit<WebSocketMessage, 'messageId' | 'timestamp'>, { getState }) => {
    const state = (getState() as RootState).websocket;
    
    if (!state.connected || !state.connection) {
      throw new Error('WebSocket not connected');
    }
    
    const fullMessage: WebSocketMessage = {
      ...message,
      messageId: `msg_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`,
      timestamp: Date.now(),
    };
    
    state.connection.send(JSON.stringify(fullMessage));
    return fullMessage;
  }
);

export const subscribeToTopic = createAsyncThunk(
  'websocket/subscribe',
  async (topic: string, { dispatch }) => {
    const message = {
      type: 'subscribe',
      payload: { topic },
    };
    
    await dispatch(sendMessage(message));
    return topic;
  }
);

export const unsubscribeFromTopic = createAsyncThunk(
  'websocket/unsubscribe',
  async (topic: string, { dispatch }) => {
    const message = {
      type: 'unsubscribe',
      payload: { topic },
    };
    
    await dispatch(sendMessage(message));
    return topic;
  }
);

export const sendPing = createAsyncThunk(
  'websocket/ping',
  async (_, { dispatch }) => {
    const message = {
      type: 'ping',
      payload: { timestamp: Date.now() },
    };
    
    dispatch(setPingTime(Date.now()));
    return dispatch(sendMessage(message));
  }
);

const websocketSlice = createSlice({
  name: 'websocket',
  initialState,
  reducers: {
    connecting: (state) => {
      state.connecting = true;
      state.error = null;
    },
    connected: (state, action: PayloadAction<WebSocket>) => {
      state.connected = true;
      state.connecting = false;
      state.connection = action.payload;
      state.error = null;
      state.reconnectAttempts = 0;
    },
    disconnected: (state, action: PayloadAction<{ code: number; reason: string }>) => {
      state.connected = false;
      state.connecting = false;
      state.connection = null;
      state.subscriptions.clear();
    },
    connectionError: (state, action: PayloadAction<string>) => {
      state.connected = false;
      state.connecting = false;
      state.connection = null;
      state.error = action.payload;
    },
    messageReceived: (state, action: PayloadAction<WebSocketMessage>) => {
      state.messageHistory.push(action.payload);
      
      // Keep only last 100 messages
      if (state.messageHistory.length > 100) {
        state.messageHistory = state.messageHistory.slice(-100);
      }
    },
    topicSubscribed: (state, action: PayloadAction<string>) => {
      state.subscriptions.add(action.payload);
    },
    topicUnsubscribed: (state, action: PayloadAction<string>) => {
      state.subscriptions.delete(action.payload);
    },
    setPingTime: (state, action: PayloadAction<number>) => {
      state.lastPingTime = action.payload;
    },
    updateLatency: (state, action: PayloadAction<number>) => {
      state.latency = action.payload;
      state.lastPingTime = null;
    },
    incrementReconnectAttempts: (state) => {
      state.reconnectAttempts += 1;
    },
    resetReconnectAttempts: (state) => {
      state.reconnectAttempts = 0;
    },
    clearMessageHistory: (state) => {
      state.messageHistory = [];
    },
  },
  extraReducers: (builder) => {
    builder
      // Initialize WebSocket
      .addCase(initializeWebSocket.pending, (state) => {
        state.connecting = true;
        state.error = null;
      })
      .addCase(initializeWebSocket.fulfilled, (state, action) => {
        // Connection handled in onopen callback
      })
      .addCase(initializeWebSocket.rejected, (state, action) => {
        state.connecting = false;
        state.error = action.error.message || 'Failed to connect';
      })
      // Subscribe to topic
      .addCase(subscribeToTopic.fulfilled, (state, action) => {
        state.subscriptions.add(action.payload);
      })
      // Unsubscribe from topic
      .addCase(unsubscribeFromTopic.fulfilled, (state, action) => {
        state.subscriptions.delete(action.payload);
      })
      // Reconnect
      .addCase(reconnect.pending, (state) => {
        state.connecting = true;
      });
  },
});

// Actions
export const {
  connecting,
  connected,
  disconnected,
  connectionError,
  messageReceived,
  topicSubscribed,
  topicUnsubscribed,
  setPingTime,
  updateLatency,
  incrementReconnectAttempts,
  resetReconnectAttempts,
  clearMessageHistory,
} = websocketSlice.actions;

// Selectors
export const selectWebSocketStatus = (state: RootState) => ({
  connected: state.websocket.connected,
  connecting: state.websocket.connecting,
  error: state.websocket.error,
  latency: state.websocket.latency,
  reconnectAttempts: state.websocket.reconnectAttempts,
});

export const selectSubscriptions = (state: RootState) => 
  Array.from(state.websocket.subscriptions);

export const selectMessageHistory = (state: RootState) => 
  state.websocket.messageHistory;

export const selectLatestMessage = (state: RootState) => 
  state.websocket.messageHistory[state.websocket.messageHistory.length - 1];

export const selectMessagesByType = (messageType: string) => (state: RootState) =>
  state.websocket.messageHistory.filter(msg => msg.type === messageType);

export default websocketSlice.reducer;