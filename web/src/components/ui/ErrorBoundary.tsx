import React, { Component, ErrorInfo, ReactNode } from 'react';
import { Box, Typography, Button, Paper } from '@mui/material';
import { motion } from 'framer-motion';
import ErrorOutlineIcon from '@mui/icons-material/ErrorOutline';
import RefreshIcon from '@mui/icons-material/Refresh';
import BugReportIcon from '@mui/icons-material/BugReport';

interface Props {
  children?: ReactNode;
  fallback?: ReactNode;
}

interface State {
  hasError: boolean;
  error: Error | null;
  errorInfo: ErrorInfo | null;
}

class ErrorBoundary extends Component<Props, State> {
  public state: State = {
    hasError: false,
    error: null,
    errorInfo: null,
  };

  public static getDerivedStateFromError(error: Error): State {
    return { hasError: true, error, errorInfo: null };
  }

  public componentDidCatch(error: Error, errorInfo: ErrorInfo) {
    console.error('Uncaught error:', error, errorInfo);
    this.setState({
      error,
      errorInfo,
    });
  }

  private handleReload = () => {
    window.location.reload();
  };

  private handleReportBug = () => {
    const { error, errorInfo } = this.state;
    const errorReport = {
      error: error?.toString(),
      stack: error?.stack,
      componentStack: errorInfo?.componentStack,
      timestamp: new Date().toISOString(),
      userAgent: navigator.userAgent,
      url: window.location.href,
    };
    
    // In a real application, you would send this to your error tracking service
    console.error('Error Report:', errorReport);
    
    // For now, copy to clipboard
    navigator.clipboard.writeText(JSON.stringify(errorReport, null, 2))
      .then(() => {
        alert('Error report copied to clipboard. Please share it with the development team.');
      })
      .catch(() => {
        alert('Failed to copy error report. Please check the console for details.');
      });
  };

  public render() {
    if (this.state.hasError) {
      if (this.props.fallback) {
        return this.props.fallback;
      }

      return (
        <Box
          sx={{
            height: '100vh',
            width: '100vw',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            background: 'linear-gradient(135deg, #0f0f23 0%, #1a1a2e 100%)',
            padding: 3,
          }}
        >
          <motion.div
            initial={{ opacity: 0, scale: 0.9 }}
            animate={{ opacity: 1, scale: 1 }}
            transition={{ duration: 0.5 }}
          >
            <Paper
              sx={{
                padding: 4,
                maxWidth: 600,
                textAlign: 'center',
                background: 'rgba(255, 255, 255, 0.05)',
                backdropFilter: 'blur(10px)',
                border: '1px solid rgba(255, 255, 255, 0.1)',
                borderRadius: 3,
              }}
            >
              <motion.div
                initial={{ scale: 0 }}
                animate={{ scale: 1 }}
                transition={{ delay: 0.2, duration: 0.3 }}
              >
                <ErrorOutlineIcon
                  sx={{
                    fontSize: 64,
                    color: '#ef4444',
                    marginBottom: 2,
                  }}
                />
              </motion.div>

              <Typography
                variant="h4"
                sx={{
                  marginBottom: 2,
                  fontWeight: 600,
                  color: '#e1e1e6',
                }}
              >
                Oops! Something went wrong
              </Typography>

              <Typography
                variant="body1"
                sx={{
                  marginBottom: 3,
                  color: '#a1a1aa',
                  lineHeight: 1.6,
                }}
              >
                MixMind AI encountered an unexpected error. This is likely a temporary issue.
                You can try reloading the application or report this bug to help us fix it.
              </Typography>

              {this.state.error && (
                <Box
                  sx={{
                    marginBottom: 3,
                    padding: 2,
                    backgroundColor: 'rgba(239, 68, 68, 0.1)',
                    border: '1px solid rgba(239, 68, 68, 0.3)',
                    borderRadius: 2,
                    textAlign: 'left',
                  }}
                >
                  <Typography
                    variant="caption"
                    sx={{
                      fontFamily: 'JetBrains Mono, monospace',
                      color: '#fecaca',
                      fontSize: '0.75rem',
                      display: 'block',
                      marginBottom: 1,
                    }}
                  >
                    Error Details:
                  </Typography>
                  <Typography
                    variant="body2"
                    sx={{
                      fontFamily: 'JetBrains Mono, monospace',
                      color: '#ef4444',
                      fontSize: '0.8rem',
                      wordBreak: 'break-word',
                    }}
                  >
                    {this.state.error.message}
                  </Typography>
                </Box>
              )}

              <Box
                sx={{
                  display: 'flex',
                  gap: 2,
                  justifyContent: 'center',
                  flexWrap: 'wrap',
                }}
              >
                <Button
                  variant="contained"
                  startIcon={<RefreshIcon />}
                  onClick={this.handleReload}
                  sx={{
                    background: 'linear-gradient(135deg, #6366f1 0%, #7c3aed 100%)',
                    '&:hover': {
                      background: 'linear-gradient(135deg, #7c3aed 0%, #8b5cf6 100%)',
                    },
                  }}
                >
                  Reload Application
                </Button>

                <Button
                  variant="outlined"
                  startIcon={<BugReportIcon />}
                  onClick={this.handleReportBug}
                  sx={{
                    borderColor: 'rgba(255, 255, 255, 0.23)',
                    color: '#e1e1e6',
                    '&:hover': {
                      borderColor: 'rgba(255, 255, 255, 0.4)',
                      backgroundColor: 'rgba(255, 255, 255, 0.05)',
                    },
                  }}
                >
                  Report Bug
                </Button>
              </Box>

              {process.env.NODE_ENV === 'development' && this.state.errorInfo && (
                <Box
                  sx={{
                    marginTop: 3,
                    padding: 2,
                    backgroundColor: 'rgba(255, 255, 255, 0.05)',
                    border: '1px solid rgba(255, 255, 255, 0.1)',
                    borderRadius: 2,
                    textAlign: 'left',
                    maxHeight: 200,
                    overflow: 'auto',
                  }}
                >
                  <Typography
                    variant="caption"
                    sx={{
                      fontFamily: 'JetBrains Mono, monospace',
                      color: '#a1a1aa',
                      fontSize: '0.75rem',
                      display: 'block',
                      marginBottom: 1,
                    }}
                  >
                    Component Stack (Development Only):
                  </Typography>
                  <Typography
                    variant="body2"
                    sx={{
                      fontFamily: 'JetBrains Mono, monospace',
                      color: '#e1e1e6',
                      fontSize: '0.7rem',
                      whiteSpace: 'pre-wrap',
                    }}
                  >
                    {this.state.errorInfo.componentStack}
                  </Typography>
                </Box>
              )}

              <Typography
                variant="caption"
                sx={{
                  display: 'block',
                  marginTop: 3,
                  color: '#6b7280',
                  fontSize: '0.75rem',
                }}
              >
                If the problem persists, please contact support with the error details above.
              </Typography>
            </Paper>
          </motion.div>
        </Box>
      );
    }

    return this.props.children;
  }
}

export default ErrorBoundary;