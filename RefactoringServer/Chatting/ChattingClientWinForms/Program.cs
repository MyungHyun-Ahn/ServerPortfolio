using System;
using System.Windows.Forms;

namespace ChattingClientWinForms;

internal static class Program
{
    [STAThread]
    private static int Main(string[] args)
    {
        if (ChattingClientSmokeRunner.IsSmokeMode(args))
        {
            return ChattingClientSmokeRunner.RunAsync(args).GetAwaiter().GetResult();
        }

        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new MainForm());
        return 0;
    }
}
