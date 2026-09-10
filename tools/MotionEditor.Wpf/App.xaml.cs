using System.IO;
using System.Windows;
using System.Windows.Threading;

namespace DX11MotionEditor;

public partial class App : Application
{
    protected override void OnStartup(StartupEventArgs e)
    {
        DispatcherUnhandledException += OnDispatcherUnhandledException;
        AppDomain.CurrentDomain.UnhandledException += OnUnhandledException;
        base.OnStartup(e);

        var window = new MainWindow();
        MainWindow = window;
        window.Show();
    }

    private static void OnDispatcherUnhandledException(object sender, DispatcherUnhandledExceptionEventArgs e)
    {
        WriteError(e.Exception);
        MessageBox.Show(
            $"モーションエディタの起動中にエラーが発生しました。\n\n{e.Exception.Message}\n\nログ: {GetLogPath()}",
            "DX11 Motion Editor",
            MessageBoxButton.OK,
            MessageBoxImage.Error);
        e.Handled = true;
        Current.Shutdown(1);
    }

    private static void OnUnhandledException(object sender, UnhandledExceptionEventArgs e)
    {
        if (e.ExceptionObject is Exception exception)
            WriteError(exception);
    }

    private static string GetLogPath() =>
        Path.Combine(Path.GetTempPath(), "DX11MotionEditor-error.log");

    private static void WriteError(Exception exception)
    {
        try
        {
            File.WriteAllText(
                GetLogPath(),
                $"{DateTime.Now:O}\n{exception}\n",
                System.Text.Encoding.UTF8);
        }
        catch
        {
            // Keep the original exception path from causing another failure.
        }
    }
}
