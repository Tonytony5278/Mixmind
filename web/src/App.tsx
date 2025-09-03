import React, { useEffect } from 'react';
import { Routes, Route, Navigate } from 'react-router-dom';
import { useDispatch } from 'react-redux';

import MainLayout from './components/layout/MainLayout';
import DAWInterface from './components/daw/DAWInterface';
import SettingsPage from './components/pages/SettingsPage';
import AnalyticsPage from './components/pages/AnalyticsPage';
import ProjectsPage from './components/pages/ProjectsPage';
import LoadingScreen from './components/ui/LoadingScreen';
import ErrorBoundary from './components/ui/ErrorBoundary';

import { useAppSelector } from './store/hooks';
import { initializeApp, selectAppStatus } from './store/slices/appSlice';
import { initializeWebSocket } from './store/slices/websocketSlice';

import './App.css';

const App: React.FC = () => {
  const dispatch = useDispatch();
  const appStatus = useAppSelector(selectAppStatus);
  
  useEffect(() => {
    // Initialize the application
    dispatch(initializeApp());
    
    // Initialize WebSocket connection
    dispatch(initializeWebSocket());
  }, [dispatch]);
  
  if (appStatus.loading) {
    return <LoadingScreen message={appStatus.loadingMessage} />;
  }
  
  if (appStatus.error) {
    return (
      <div className="error-screen">
        <h1>Application Error</h1>
        <p>{appStatus.error}</p>
        <button onClick={() => window.location.reload()}>Reload</button>
      </div>
    );
  }
  
  return (
    <ErrorBoundary>
      <div className="App">
        <Routes>
          <Route path="/" element={<MainLayout />}>
            <Route index element={<Navigate to="/daw" replace />} />
            <Route path="daw" element={<DAWInterface />} />
            <Route path="projects" element={<ProjectsPage />} />
            <Route path="analytics" element={<AnalyticsPage />} />
            <Route path="settings" element={<SettingsPage />} />
          </Route>
        </Routes>
      </div>
    </ErrorBoundary>
  );
};

export default App;