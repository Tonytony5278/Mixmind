import { configureStore } from '@reduxjs/toolkit';
import { setupListeners } from '@reduxjs/toolkit/query';

// Slice imports
import appSlice from './slices/appSlice';
import websocketSlice from './slices/websocketSlice';
import sessionSlice from './slices/sessionSlice';
import transportSlice from './slices/transportSlice';
import trackSlice from './slices/trackSlice';
import pluginSlice from './slices/pluginSlice';
import mixerSlice from './slices/mixerSlice';
import aiSlice from './slices/aiSlice';
import uiSlice from './slices/uiSlice';
import settingsSlice from './slices/settingsSlice';

// API slice import
import { apiSlice } from './api/apiSlice';

export const store = configureStore({
  reducer: {
    app: appSlice,
    websocket: websocketSlice,
    session: sessionSlice,
    transport: transportSlice,
    tracks: trackSlice,
    plugins: pluginSlice,
    mixer: mixerSlice,
    ai: aiSlice,
    ui: uiSlice,
    settings: settingsSlice,
    api: apiSlice.reducer,
  },
  middleware: (getDefaultMiddleware) =>
    getDefaultMiddleware({
      serializableCheck: {
        // Ignore these action types
        ignoredActions: [
          'websocket/connect',
          'websocket/disconnect',
          'websocket/messageReceived',
          'api/executeGenerated/pending',
          'api/executeGenerated/fulfilled',
          'api/executeGenerated/rejected',
        ],
        // Ignore these field paths in all actions
        ignoredActionsPaths: ['payload.timestamp', 'payload.socket'],
        // Ignore these paths in the state
        ignoredPaths: ['websocket.connection', 'api.queries'],
      },
    }).concat(apiSlice.middleware),
  devTools: process.env.NODE_ENV !== 'production',
});

// Optional: Setup listeners for refetchOnFocus/refetchOnReconnect behaviors
setupListeners(store.dispatch);

export type RootState = ReturnType<typeof store.getState>;
export type AppDispatch = typeof store.dispatch;