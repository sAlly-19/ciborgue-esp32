import { createContext, useContext } from 'react';

export type Confirmation = {
  title: string;
  message: string;
  action: () => void;
  intent: 'default' | 'danger';
};

export type Toast = { message: string; tone: 'success' | 'error' | 'info' };

export const FeedbackContext = createContext<{
  confirmation: Confirmation | null;
  setConfirmation: (confirmation: Confirmation | null) => void;
  toast: Toast | null;
  showToast: (toast: Toast) => void;
  dismissToast: () => void;
  sheetCount: number;
  registerSheet: () => () => void;
} | null>(null);

export const useFeedback = () => useContext(FeedbackContext);
export function useConfirm(intent: Confirmation['intent'] = 'default') {
  const feedback = useFeedback();
  return (title: string, message: string, action: () => void) => {
    feedback?.setConfirmation({ title, message, action, intent });
  };
}
