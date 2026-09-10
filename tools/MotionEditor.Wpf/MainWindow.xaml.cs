using System.IO;
using System.Globalization;
using System.Numerics;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Media3D;
using System.Windows.Shapes;
using System.Windows.Threading;
using Microsoft.Win32;
using Path = System.IO.Path;
using NumericsQuaternion = System.Numerics.Quaternion;

namespace DX11MotionEditor;

public partial class MainWindow : Window
{
    private readonly MotionDocument _document = new();
    private readonly MotionDocument _basePose = new();
    private readonly GltfPreviewModel _previewModel = new();
    private readonly Stack<MotionSnapshot> _undo = new();
    private readonly Stack<MotionSnapshot> _redo = new();
    private readonly DispatcherTimer _playTimer;
    private readonly List<string> _motionFiles = new();
    private string _projectRoot = string.Empty;
    private string _previewModelPath = string.Empty;
    private string _selectedBone = string.Empty;
    private MotionSnapshot? _editBefore;
    private bool _suppressUi;
    private bool _suppressFileSelection;
    private bool _suppressBoneSelection;
    private bool _timelineDragging;
    private float _timelineDragTime;
    private bool _poseDragging;
    private bool _cameraOrbiting;
    private bool _cameraPanning;
    private Point _lastPosePoint;
    private readonly Dictionary<Model3D, string> _poseModelBones = new();
    private readonly Dictionary<string, Point3D> _posePoints = new(StringComparer.Ordinal);
    private double _cameraYaw;
    private double _cameraPitch;
    private double _cameraDistance = 5.5;
    private Vector3D _cameraTarget = new(0.0, 0.75, 0.0);
    private bool _modelUsesZUpCoordinates;
    private int _gizmoMode;

    public MainWindow()
    {
        InitializeComponent();
        _projectRoot = FindProjectRoot();
        _document.Changed += DocumentChanged;
        _playTimer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1.0 / 60.0) };
        _playTimer.Tick += PlayTimerTick;
        Loaded += WindowLoaded;
        Closing += (_, _) => _playTimer.Stop();
    }

    private void WindowLoaded(object sender, RoutedEventArgs e)
    {
        ReloadMotionFiles();
        var basePosePath = Path.Combine(_projectRoot, "assets", "motion", "sword_shield_attack_safe.motion");
        if (File.Exists(basePosePath))
            _basePose.Load(basePosePath);
        var defaultModel = Path.Combine(_projectRoot, "assets", "model", "SwordShieldPack", "runtime", "SwordShieldPack_Player_fixed.glb");
        if (File.Exists(defaultModel))
            LoadPreviewModel(defaultModel, false);
        var defaultFile = Path.Combine(_projectRoot, "assets", "motion", "sword_shield_attack_safe.motion");
        var initialFile = File.Exists(defaultFile)
            ? defaultFile
            : _motionFiles.FirstOrDefault() ?? string.Empty;
        if (!string.IsNullOrEmpty(initialFile))
            LoadDocument(initialFile);
        else
            SetStatus("assets/motion に .motion ファイルがありません。");
    }

    private void DocumentChanged(object? sender, EventArgs e)
    {
        RefreshView();
        if (!_suppressUi && AutoSaveCheckBox.IsChecked == true && !string.IsNullOrWhiteSpace(_document.FilePath))
            SaveCurrent(false);
    }

    private void ReloadMotionFiles()
    {
        var directory = Path.Combine(_projectRoot, "assets", "motion");
        _motionFiles.Clear();
        if (Directory.Exists(directory))
        {
            _motionFiles.AddRange(Directory.EnumerateFiles(directory, "*.motion")
                .OrderBy(path => Path.GetFileName(path), StringComparer.OrdinalIgnoreCase));
        }

        _suppressFileSelection = true;
        MotionFilesList.ItemsSource = null;
        MotionFilesList.ItemsSource = _motionFiles.Select(path => Path.GetFileName(path)).ToList();
        var selectedIndex = -1;
        if (!string.IsNullOrWhiteSpace(_document.FilePath))
        {
            selectedIndex = _motionFiles.FindIndex(path =>
                string.Equals(Path.GetFullPath(path), Path.GetFullPath(_document.FilePath), StringComparison.OrdinalIgnoreCase));
        }
        if (selectedIndex >= 0)
            MotionFilesList.SelectedIndex = selectedIndex;
        _suppressFileSelection = false;
    }

    private void LoadDocument(string path)
    {
        try
        {
            _suppressUi = true;
            _document.Load(path);
            _undo.Clear();
            _redo.Clear();
            _selectedBone = string.Empty;
            RefreshBoneList();
            if (BonesList.Items.Count > 0)
                BonesList.SelectedIndex = 0;
            _suppressUi = false;
            RefreshView();
            SelectMotionListItem(path);
            SetStatus($"読み込み: {Path.GetFileName(path)}");
        }
        catch (Exception ex)
        {
            _suppressUi = false;
            MessageBox.Show(this, ex.Message, "モーション読み込みエラー", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void SelectMotionListItem(string path)
    {
        var index = _motionFiles.FindIndex(item =>
            string.Equals(Path.GetFullPath(item), Path.GetFullPath(path), StringComparison.OrdinalIgnoreCase));
        if (index < 0)
            return;
        _suppressFileSelection = true;
        MotionFilesList.SelectedIndex = index;
        _suppressFileSelection = false;
    }

    private void RefreshBoneList()
    {
        _suppressBoneSelection = true;
        BonesList.ItemsSource = _document.BoneNames;
        var boneNames = _document.BoneNames.ToList();
        var selectedIndex = boneNames.IndexOf(_selectedBone);
        if (selectedIndex < 0 && _document.BoneNames.Count > 0)
        {
            _selectedBone = boneNames[0];
            selectedIndex = 0;
        }
        BonesList.SelectedIndex = selectedIndex;
        _suppressBoneSelection = false;
    }

    private void RefreshView()
    {
        if (!IsInitialized)
            return;
        CurrentMotionText.Text = string.IsNullOrWhiteSpace(_document.FilePath)
            ? "モーション未読み込み"
            : Path.GetFileName(_document.FilePath);
        FileStatusText.Text = _document.FilePath;
        SelectedBoneText.Text = string.IsNullOrWhiteSpace(_selectedBone)
            ? "ボーン未選択"
            : _selectedBone;
        TimeSlider.Maximum = Math.Max(_document.Duration, 0.01f);
        _suppressUi = true;
        TimeSlider.Value = Math.Clamp(_document.CurrentTime, 0.0f, TimeSlider.Maximum);
        _suppressUi = false;
        TimeText.Text = $"{_document.CurrentTime:0.000} / {_document.Duration:0.000} sec  ({_document.CurrentTime * 60.0f:0} frame)";
        RefreshFields();
        DrawTimeline();
        DrawPosePreview();
    }

    private void RefreshFields()
    {
        var key = string.IsNullOrWhiteSpace(_selectedBone)
            ? null
            : _document.SampleKey(_selectedBone, _document.CurrentTime);
        key ??= new MotionKey { Time = _document.CurrentTime, Scale = Vector3.One };
        var degrees = key.Rotation * (180.0f / MathF.PI);
        _suppressUi = true;
        SetText(RotationXBox, degrees.X);
        SetText(RotationYBox, degrees.Y);
        SetText(RotationZBox, degrees.Z);
        SetText(PositionXBox, key.Position.X);
        SetText(PositionYBox, key.Position.Y);
        SetText(PositionZBox, key.Position.Z);
        SetText(ScaleXBox, key.Scale.X);
        SetText(ScaleYBox, key.Scale.Y);
        SetText(ScaleZBox, key.Scale.Z);
        _suppressUi = false;
    }

    private static void SetText(TextBox box, float value)
    {
        box.Text = value.ToString("0.####", CultureInfo.CurrentCulture);
    }

    private void RefreshMotionAndSelection()
    {
        RefreshBoneList();
        RefreshView();
    }

    private void MotionFileSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_suppressFileSelection || MotionFilesList.SelectedIndex < 0 || MotionFilesList.SelectedIndex >= _motionFiles.Count)
            return;
        LoadDocument(_motionFiles[MotionFilesList.SelectedIndex]);
    }

    private void BoneSelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_suppressBoneSelection || BonesList.SelectedItem is not string boneName)
            return;
        _selectedBone = boneName;
        RefreshView();
    }

    private void TimeSliderChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
    {
        if (_suppressUi)
            return;
        _document.CurrentTime = (float)e.NewValue;
        RefreshFields();
        DrawTimeline();
        DrawPosePreview();
    }

    private void OpenClick(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "Motion files (*.motion)|*.motion|All files (*.*)|*.*",
            InitialDirectory = Path.Combine(_projectRoot, "assets", "motion")
        };
        if (dialog.ShowDialog(this) == true)
        {
            if (!_motionFiles.Contains(dialog.FileName, StringComparer.OrdinalIgnoreCase))
            {
                _motionFiles.Add(dialog.FileName);
                _motionFiles.Sort(StringComparer.OrdinalIgnoreCase);
                MotionFilesList.ItemsSource = _motionFiles.Select(Path.GetFileName).ToList();
            }
            LoadDocument(dialog.FileName);
        }
    }

    private void OpenModelClick(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "GLBモデル (*.glb)|*.glb|すべてのファイル (*.*)|*.*",
            InitialDirectory = Path.Combine(_projectRoot, "assets", "model")
        };
        if (dialog.ShowDialog(this) == true)
            LoadPreviewModel(dialog.FileName, true);
    }

    private void LoadPreviewModel(string path, bool showError)
    {
        if (_previewModel.Load(path))
        {
            _previewModelPath = _previewModel.FilePath;
            _modelUsesZUpCoordinates = false;
            ModelStatusText.Text = $"モデル: {Path.GetFileName(path)}";
            Title = $"DX11 Motion Editor - {Path.GetFileName(path)}";
            SetStatus($"モデル読込: {Path.GetFileName(path)}");
            DrawPosePreview();
            return;
        }

        ModelStatusText.Text = "モデル: 読込失敗（3Dスケルトンを表示）";
        _modelUsesZUpCoordinates = false;
        Title = "DX11 Motion Editor - Skeleton Preview";
        if (showError)
            MessageBox.Show(this, _previewModel.ErrorText, "モデル読込エラー", MessageBoxButton.OK, MessageBoxImage.Error);
    }

    private void SaveClick(object sender, RoutedEventArgs e) => SaveCurrent(true);

    private void SaveAsClick(object sender, RoutedEventArgs e)
    {
        var dialog = new SaveFileDialog
        {
            Filter = "Motion files (*.motion)|*.motion|All files (*.*)|*.*",
            DefaultExt = ".motion",
            FileName = string.IsNullOrWhiteSpace(_document.FilePath)
                ? "new_motion.motion"
                : Path.GetFileName(_document.FilePath),
            InitialDirectory = Path.Combine(_projectRoot, "assets", "motion")
        };
        if (dialog.ShowDialog(this) == true)
        {
            SaveTo(dialog.FileName, true);
            ReloadMotionFiles();
            SelectMotionListItem(dialog.FileName);
        }
    }

    private void SaveCurrent(bool showStatus)
    {
        if (string.IsNullOrWhiteSpace(_document.FilePath))
        {
            SaveAsClick(this, new RoutedEventArgs());
            return;
        }
        SaveTo(_document.FilePath, showStatus);
    }

    private void SaveTo(string path, bool showStatus)
    {
        try
        {
            _document.Save(path);
            WriteSelectedAttackFile(path);
            if (showStatus)
                SetStatus($"保存: {Path.GetFileName(path)}");
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, ex.Message, "モーション保存エラー", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private void WriteSelectedAttackFile(string path)
    {
        var motionDirectory = Path.Combine(_projectRoot, "assets", "motion");
        var fullPath = Path.GetFullPath(path);
        if (!fullPath.StartsWith(Path.GetFullPath(motionDirectory), StringComparison.OrdinalIgnoreCase))
            return;
        var relative = Path.GetRelativePath(_projectRoot, fullPath).Replace('\\', '/');
        File.WriteAllText(Path.Combine(motionDirectory, "selected_attack.txt"), relative + Environment.NewLine);
    }

    private void ReloadFilesClick(object sender, RoutedEventArgs e)
    {
        var current = _document.FilePath;
        ReloadMotionFiles();
        if (!string.IsNullOrWhiteSpace(current) && File.Exists(current))
            LoadDocument(current);
    }

    private void ExitClick(object sender, RoutedEventArgs e) => Close();

    private void AddKeyClick(object sender, RoutedEventArgs e)
    {
        ExecuteEdit(() => ApplyCurrentKeyFromFields(false));
    }

    private void DeleteKeyClick(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_selectedBone))
            return;
        ExecuteEdit(() => _document.DeleteKey(_selectedBone, _document.CurrentTime));
    }

    private void DuplicateKeyClick(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(_selectedBone))
            return;
        ExecuteEdit(() => _document.DuplicateKey(_selectedBone, _document.CurrentTime));
    }

    private void UndoClick(object sender, RoutedEventArgs e)
    {
        if (_undo.Count == 0)
            return;
        _redo.Push(_document.Capture());
        _document.Restore(_undo.Pop());
        SetStatus("Undo");
    }

    private void RedoClick(object sender, RoutedEventArgs e)
    {
        if (_redo.Count == 0)
            return;
        _undo.Push(_document.Capture());
        _document.Restore(_redo.Pop());
        SetStatus("Redo");
    }

    private void ExecuteEdit(Func<bool> edit)
    {
        var before = _document.Capture();
        if (!edit())
            return;
        var after = _document.Capture();
        if (before.Signature == after.Signature)
            return;
        _undo.Push(before);
        _redo.Clear();
        _document.Touch();
    }

    private void BeginEditTransaction()
    {
        _editBefore ??= _document.Capture();
    }

    private void EndEditTransaction()
    {
        if (_editBefore is null)
            return;
        var before = _editBefore;
        _editBefore = null;
        var after = _document.Capture();
        if (before.Signature == after.Signature)
            return;
        _undo.Push(before);
        _redo.Clear();
        _document.Touch();
    }

    private void BeginFieldEdit(object sender, KeyboardFocusChangedEventArgs e) => BeginEditTransaction();

    private void EndFieldEdit(object sender, KeyboardFocusChangedEventArgs e) => EndEditTransaction();

    private void NumericFieldChanged(object sender, TextChangedEventArgs e)
    {
        if (_suppressUi || string.IsNullOrWhiteSpace(_selectedBone))
            return;
        if (TryReadFields(out var key))
        {
            BeginEditTransaction();
            _document.UpsertKey(_selectedBone, key);
            RefreshTimelineAndPoseOnly();
        }
    }

    private bool TryReadFields(out MotionKey key)
    {
        key = new MotionKey { Time = _document.CurrentTime, Scale = Vector3.One };
        if (!TryFloat(RotationXBox, out var rx) || !TryFloat(RotationYBox, out var ry) || !TryFloat(RotationZBox, out var rz) ||
            !TryFloat(PositionXBox, out var px) || !TryFloat(PositionYBox, out var py) || !TryFloat(PositionZBox, out var pz) ||
            !TryFloat(ScaleXBox, out var sx) || !TryFloat(ScaleYBox, out var sy) || !TryFloat(ScaleZBox, out var sz))
            return false;
        key.Rotation = new Vector3(rx, ry, rz) * (MathF.PI / 180.0f);
        key.Position = new Vector3(px, py, pz);
        key.Scale = new Vector3(Math.Max(sx, 0.01f), Math.Max(sy, 0.01f), Math.Max(sz, 0.01f));
        return true;
    }

    private static bool TryFloat(TextBox box, out float value) =>
        float.TryParse(box.Text, NumberStyles.Float, CultureInfo.CurrentCulture, out value) && float.IsFinite(value);

    private bool ApplyCurrentKeyFromFields(bool touch)
    {
        if (string.IsNullOrWhiteSpace(_selectedBone) || !TryReadFields(out var key))
            return false;
        var changed = _document.UpsertKey(_selectedBone, key);
        if (touch && changed)
            _document.Touch();
        return changed;
    }

    private void GizmoModeChanged(object sender, RoutedEventArgs e)
    {
        if (sender is RadioButton radio && int.TryParse(radio.Tag?.ToString(), out var mode))
            _gizmoMode = mode;
    }

    private void PlayClick(object sender, RoutedEventArgs e)
    {
        if (_playTimer.IsEnabled)
        {
            _playTimer.Stop();
            SetStatus("一時停止");
        }
        else
        {
            _playTimer.Start();
            SetStatus("再生中");
        }
    }

    private void StopClick(object sender, RoutedEventArgs e)
    {
        _playTimer.Stop();
        SetCurrentTime(0.0f);
        SetStatus("停止");
    }

    private void PlayTimerTick(object? sender, EventArgs e)
    {
        var next = _document.CurrentTime + 1.0f / 60.0f;
        if (next > _document.Duration)
        {
            if (LoopCheckBox.IsChecked == true)
                next = 0.0f;
            else
            {
                next = _document.Duration;
                _playTimer.Stop();
            }
        }
        SetCurrentTime(next);
    }

    private void SetCurrentTime(float time)
    {
        _document.CurrentTime = Math.Clamp(time, 0.0f, Math.Max(_document.Duration, 0.01f));
        _suppressUi = true;
        TimeSlider.Value = _document.CurrentTime;
        _suppressUi = false;
        RefreshFields();
        TimeText.Text = $"{_document.CurrentTime:0.000} / {_document.Duration:0.000} sec  ({_document.CurrentTime * 60.0f:0} frame)";
        DrawTimeline();
        DrawPosePreview();
    }

    private void TimelineSizeChanged(object sender, SizeChangedEventArgs e) => DrawTimeline();

    private float TimelineTimeFromX(double x)
    {
        var left = 12.0;
        var right = Math.Max(left + 1.0, TimelineCanvas.ActualWidth - 12.0);
        return (float)Math.Clamp((x - left) / (right - left) * Math.Max(_document.Duration, 0.01f), 0.0, Math.Max(_document.Duration, 0.01f));
    }

    private MotionKey? TimelineKeyAt(Point point)
    {
        if (string.IsNullOrWhiteSpace(_selectedBone) || !_document.Bones.TryGetValue(_selectedBone, out var keys))
            return null;
        var left = 12.0;
        var right = Math.Max(left + 1.0, TimelineCanvas.ActualWidth - 12.0);
        var span = Math.Max(_document.Duration, 0.01f);
        return keys.OrderBy(key => Math.Abs((left + key.Time / span * (right - left)) - point.X))
            .FirstOrDefault(key => Math.Abs((left + key.Time / span * (right - left)) - point.X) <= 9.0);
    }

    private void TimelineMouseDown(object sender, MouseButtonEventArgs e)
    {
        var point = e.GetPosition(TimelineCanvas);
        var key = TimelineKeyAt(point);
        if (key is not null)
        {
            _timelineDragging = true;
            _timelineDragTime = key.Time;
            BeginEditTransaction();
            SetCurrentTime(key.Time);
            TimelineCanvas.CaptureMouse();
        }
        else
            SetCurrentTime(TimelineTimeFromX(point.X));
        e.Handled = true;
    }

    private void TimelineMouseMove(object sender, MouseEventArgs e)
    {
        if (!_timelineDragging || e.LeftButton != MouseButtonState.Pressed)
            return;
        var time = TimelineTimeFromX(e.GetPosition(TimelineCanvas).X);
        if (_document.MoveKey(_selectedBone, _timelineDragTime, time))
        {
            _timelineDragTime = time;
            _document.CurrentTime = time;
            RefreshTimelineAndPoseOnly();
        }
    }

    private void TimelineMouseUp(object sender, MouseButtonEventArgs e)
    {
        if (!_timelineDragging)
            return;
        _timelineDragging = false;
        TimelineCanvas.ReleaseMouseCapture();
        EndEditTransaction();
        e.Handled = true;
    }

    private void TimelineRightMouseDown(object sender, MouseButtonEventArgs e)
    {
        var key = TimelineKeyAt(e.GetPosition(TimelineCanvas));
        if (key is not null)
            ExecuteEdit(() => _document.DeleteKey(_selectedBone, key.Time));
        e.Handled = true;
    }

    private void RefreshTimelineAndPoseOnly()
    {
        RefreshFields();
        DrawTimeline();
        DrawPosePreview();
    }

    private void PoseViewportSizeChanged(object sender, SizeChangedEventArgs e)
    {
        UpdateCamera();
        DrawPosePreview();
    }

    private void PoseViewportMouseDown(object sender, MouseButtonEventArgs e)
    {
        PoseViewport.Focus();
        _lastPosePoint = e.GetPosition(PoseViewport);

        if (e.ChangedButton == MouseButton.Middle ||
            (e.ChangedButton == MouseButton.Left &&
             (Keyboard.IsKeyDown(Key.LeftAlt) || Keyboard.IsKeyDown(Key.RightAlt))))
        {
            _cameraOrbiting = true;
            PoseViewport.CaptureMouse();
            e.Handled = true;
            return;
        }

        if (e.ChangedButton == MouseButton.Right)
        {
            _cameraPanning = true;
            PoseViewport.CaptureMouse();
            e.Handled = true;
            return;
        }

        if (e.ChangedButton != MouseButton.Left)
            return;

        if ((Keyboard.IsKeyDown(Key.LeftShift) || Keyboard.IsKeyDown(Key.RightShift)) &&
            !string.IsNullOrWhiteSpace(_selectedBone))
        {
            _poseDragging = true;
            BeginEditTransaction();
            PoseViewport.CaptureMouse();
            e.Handled = true;
            return;
        }

        var hitBone = FindHitBone(_lastPosePoint);
        if (!string.IsNullOrWhiteSpace(hitBone))
            SelectBoneFromViewport(hitBone);

        if (string.IsNullOrWhiteSpace(hitBone))
        {
            _cameraOrbiting = true;
            PoseViewport.CaptureMouse();
            e.Handled = true;
            return;
        }

        _poseDragging = true;
        BeginEditTransaction();
        PoseViewport.CaptureMouse();
        e.Handled = true;
    }

    private void PoseViewportMouseMove(object sender, MouseEventArgs e)
    {
        var point = e.GetPosition(PoseViewport);
        var dx = point.X - _lastPosePoint.X;
        var dy = point.Y - _lastPosePoint.Y;
        _lastPosePoint = point;

        if (_cameraOrbiting && e.LeftButton == MouseButtonState.Pressed ||
            _cameraOrbiting && e.MiddleButton == MouseButtonState.Pressed)
        {
            _cameraYaw -= dx * 0.01;
            // Keep the editor above/below the character only within a useful
            // orbit range. This prevents the old floor plane from occluding
            // the model when the camera was dragged past the feet.
            _cameraPitch = Math.Clamp(_cameraPitch + dy * 0.01, -0.95, 0.95);
            UpdateCamera();
            return;
        }

        if (_cameraPanning && e.RightButton == MouseButtonState.Pressed)
        {
            var look = PoseCamera.LookDirection;
            look.Normalize();
            var right = Vector3D.CrossProduct(look, PoseCamera.UpDirection);
            right.Normalize();
            var up = PoseCamera.UpDirection;
            up.Normalize();
            var panAmount = _cameraDistance * 0.0018;
            _cameraTarget -= right * (dx * panAmount);
            _cameraTarget += up * (dy * panAmount);
            UpdateCamera();
            return;
        }

        if (!_poseDragging || e.LeftButton != MouseButtonState.Pressed || string.IsNullOrWhiteSpace(_selectedBone))
            return;

        var key = _document.SampleKey(_selectedBone, _document.CurrentTime)?.Clone() ??
            new MotionKey { Time = _document.CurrentTime, Scale = Vector3.One };
        if (_gizmoMode == 0)
        {
            if (Keyboard.IsKeyDown(Key.LeftCtrl) || Keyboard.IsKeyDown(Key.RightCtrl))
                key.Rotation = new Vector3(key.Rotation.X, key.Rotation.Y, key.Rotation.Z + (float)dx * 0.01f);
            else
                key.Rotation = new Vector3(key.Rotation.X - (float)dy * 0.01f, key.Rotation.Y + (float)dx * 0.01f, key.Rotation.Z);
        }
        else if (_gizmoMode == 1)
            key.Position += new Vector3((float)dx * 0.01f, (float)-dy * 0.01f, 0.0f);
        else
        {
            var amount = (float)(dx - dy) * 0.003f;
            key.Scale = Vector3.Max(new Vector3(0.01f), key.Scale + new Vector3(amount));
        }
        _document.UpsertKey(_selectedBone, key);
        RefreshTimelineAndPoseOnly();
    }

    private void PoseViewportMouseUp(object sender, MouseButtonEventArgs e)
    {
        if (_poseDragging)
        {
            _poseDragging = false;
            EndEditTransaction();
        }
        if ((e.ChangedButton == MouseButton.Left && _cameraOrbiting) ||
            (e.ChangedButton == MouseButton.Middle && _cameraOrbiting))
            _cameraOrbiting = false;
        if (e.ChangedButton == MouseButton.Right)
            _cameraPanning = false;
        if (!_poseDragging && !_cameraOrbiting && !_cameraPanning)
            PoseViewport.ReleaseMouseCapture();
        e.Handled = true;
    }

    private void PoseViewportMouseWheel(object sender, MouseWheelEventArgs e)
    {
        _cameraDistance = Math.Clamp(_cameraDistance * Math.Pow(0.88, e.Delta / 120.0), 1.6, 18.0);
        UpdateCamera();
        e.Handled = true;
    }

    private void ResetViewClick(object sender, RoutedEventArgs e)
    {
        _cameraYaw = 0.0;
        _cameraPitch = 0.0;
        _cameraDistance = 5.5;
        _cameraTarget = new Vector3D(0.0, 0.75, 0.0);
        UpdateCamera();
    }

    private void ShowSkeletonClick(object sender, RoutedEventArgs e) => DrawPosePreview();

    private void DrawTimeline()
    {
        if (TimelineCanvas is null)
            return;
        TimelineCanvas.Children.Clear();
        var width = Math.Max(TimelineCanvas.ActualWidth, 80.0);
        var height = Math.Max(TimelineCanvas.ActualHeight, 60.0);
        var left = 12.0;
        var right = width - 12.0;
        var span = Math.Max(_document.Duration, 0.01f);
        AddLine(TimelineCanvas, left, height - 18, right, height - 18, Color.FromRgb(115, 128, 145), 1);
        var maxFrame = (int)Math.Ceiling(span * 60.0f);
        for (var frame = 0; frame <= maxFrame; frame += 5)
        {
            var x = left + frame / 60.0 / span * (right - left);
            AddLine(TimelineCanvas, x, 10, x, height - 18, Color.FromArgb(100, 105, 120, 135), 1);
            AddText(TimelineCanvas, $"{frame}", x + 2, 2, 10, Brushes.LightGray);
        }
        if (!string.IsNullOrWhiteSpace(_selectedBone) && _document.Bones.TryGetValue(_selectedBone, out var keys))
        {
            foreach (var key in keys)
            {
                var x = left + key.Time / span * (right - left);
                var ellipse = new Ellipse
                {
                    Width = 11,
                    Height = 11,
                    Fill = Math.Abs(key.Time - _document.CurrentTime) < 0.02f
                        ? new SolidColorBrush(Color.FromRgb(244, 197, 66))
                        : new SolidColorBrush(Color.FromRgb(66, 184, 232)),
                    Stroke = Brushes.White,
                    StrokeThickness = 1
                };
                Canvas.SetLeft(ellipse, x - 5.5);
                Canvas.SetTop(ellipse, height - 34);
                TimelineCanvas.Children.Add(ellipse);
            }
        }
        var currentX = left + _document.CurrentTime / span * (right - left);
        AddLine(TimelineCanvas, currentX, 0, currentX, height, Color.FromRgb(240, 100, 80), 2);
    }

    private void DrawPosePreview()
    {
        if (PoseViewport is null || PoseWorld is null)
            return;
        PoseWorld.Children.Clear();
        _poseModelBones.Clear();
        _posePoints.Clear();

        var order = new[]
        {
            "hips", "spine", "spine1", "spine2", "neck", "head",
            "leftshoulder", "leftarm", "leftforearm", "lefthand",
            "rightshoulder", "rightarm", "rightforearm", "righthand",
            "leftupleg", "leftleg", "leftfoot", "rightupleg", "rightleg", "rightfoot"
        };
        var hierarchy = new Dictionary<string, (string Parent, Vector3 Offset)>(StringComparer.Ordinal)
        {
            ["hips"] = (string.Empty, new Vector3(0.0f, 0.0f, 0.0f)),
            ["spine"] = ("hips", new Vector3(0.0f, 0.28f, 0.0f)),
            ["spine1"] = ("spine", new Vector3(0.0f, 0.28f, 0.0f)),
            ["spine2"] = ("spine1", new Vector3(0.0f, 0.27f, 0.0f)),
            ["neck"] = ("spine2", new Vector3(0.0f, 0.20f, 0.0f)),
            ["head"] = ("neck", new Vector3(0.0f, 0.25f, 0.0f)),
            ["leftshoulder"] = ("spine2", new Vector3(-0.20f, 0.02f, 0.0f)),
            ["leftarm"] = ("leftshoulder", new Vector3(-0.30f, -0.04f, 0.0f)),
            ["leftforearm"] = ("leftarm", new Vector3(-0.27f, 0.0f, 0.0f)),
            ["lefthand"] = ("leftforearm", new Vector3(-0.18f, 0.0f, 0.0f)),
            ["rightshoulder"] = ("spine2", new Vector3(0.20f, 0.02f, 0.0f)),
            ["rightarm"] = ("rightshoulder", new Vector3(0.30f, -0.04f, 0.0f)),
            ["rightforearm"] = ("rightarm", new Vector3(0.27f, 0.0f, 0.0f)),
            ["righthand"] = ("rightforearm", new Vector3(0.18f, 0.0f, 0.0f)),
            ["leftupleg"] = ("hips", new Vector3(-0.13f, -0.30f, 0.0f)),
            ["leftleg"] = ("leftupleg", new Vector3(0.0f, -0.42f, 0.0f)),
            ["leftfoot"] = ("leftleg", new Vector3(-0.01f, -0.18f, 0.08f)),
            ["rightupleg"] = ("hips", new Vector3(0.13f, -0.30f, 0.0f)),
            ["rightleg"] = ("rightupleg", new Vector3(0.0f, -0.42f, 0.0f)),
            ["rightfoot"] = ("rightleg", new Vector3(0.01f, -0.18f, 0.08f))
        };

        var actualNames = order.ToDictionary(alias => alias, FindBoneForAlias, StringComparer.Ordinal);
        var rotations = new Dictionary<string, NumericsQuaternion>(StringComparer.Ordinal)
        {
            ["hips"] = NumericsQuaternion.Identity
        };
        var scales = new Dictionary<string, Vector3>(StringComparer.Ordinal)
        {
            ["hips"] = Vector3.One
        };

        var rootKey = string.IsNullOrEmpty(actualNames["hips"])
            ? null
            : _document.SampleKey(actualNames["hips"], _document.CurrentTime);
        var rootPosition = new Vector3(0.0f, 0.82f, 0.0f) + (rootKey?.Position ?? Vector3.Zero);
        _posePoints["hips"] = ToPoint3D(rootPosition);
        rotations["hips"] = ToQuaternion(rootKey?.Rotation ?? Vector3.Zero);
        scales["hips"] = SanitizeScale(rootKey?.Scale ?? Vector3.One);

        foreach (var alias in order.Skip(1))
        {
            var info = hierarchy[alias];
            var parentPosition = _posePoints[info.Parent];
            var parentRotation = rotations[info.Parent];
            var parentScale = scales[info.Parent];
            var actualName = actualNames[alias];
            var key = string.IsNullOrEmpty(actualName)
                ? null
                : _document.SampleKey(actualName, _document.CurrentTime);
            var localPosition = info.Offset + (key?.Position ?? Vector3.Zero);
            var localScale = SanitizeScale(key?.Scale ?? Vector3.One);
            var offset = Vector3.Transform(localPosition * parentScale, parentRotation);
            var position = new Vector3(
                (float)parentPosition.X + offset.X,
                (float)parentPosition.Y + offset.Y,
                (float)parentPosition.Z + offset.Z);
            _posePoints[alias] = ToPoint3D(position);
            rotations[alias] = NumericsQuaternion.Normalize(parentRotation * ToQuaternion(key?.Rotation ?? Vector3.Zero));
            scales[alias] = SanitizeScale(parentScale * localScale);
        }

        PoseWorld.Children.Add(new AmbientLight(Color.FromRgb(72, 78, 92)));
        PoseWorld.Children.Add(new DirectionalLight(Color.FromRgb(230, 235, 245), new Vector3D(-1.0, -2.0, -3.0)));
        PoseWorld.Children.Add(new DirectionalLight(Color.FromRgb(90, 125, 170), new Vector3D(1.0, -0.5, 1.0)));
        if (_previewModel.IsLoaded)
        {
            var model = _previewModel.BuildPose(
                _document.CurrentTime,
                (boneName, time) => _document.SampleKey(boneName, time),
                _document.FirstKey,
                _basePose.FirstKey,
                IsImportedMotion());
            PoseWorld.Children.Add(model);
        }

        var showSkeleton = !_previewModel.IsLoaded || ShowSkeletonCheckBox.IsChecked == true;
        if (showSkeleton)
        {
            foreach (var alias in order.Skip(1))
            {
                var parent = hierarchy[alias].Parent;
                var selected = actualNames[alias] == _selectedBone || actualNames[parent] == _selectedBone;
                var color = selected
                    ? Color.FromRgb(226, 183, 58)
                    : _previewModel.IsLoaded
                        ? Color.FromArgb(48, 70, 135, 185)
                        : Color.FromRgb(70, 135, 185);
                var model = new GeometryModel3D(
                    CreatePrismMesh(_posePoints[parent], _posePoints[alias], selected ? 0.075 : 0.055, selected ? 0.075 : 0.055),
                    CreateMaterial(color));
                PoseWorld.Children.Add(model);
                if (!string.IsNullOrEmpty(actualNames[alias]))
                    _poseModelBones[model] = actualNames[alias];
            }

            foreach (var alias in order)
            {
                var selected = !string.IsNullOrEmpty(actualNames[alias]) && actualNames[alias] == _selectedBone;
                var jointColor = selected
                    ? Color.FromRgb(244, 197, 66)
                    : _previewModel.IsLoaded
                        ? Color.FromArgb(55, 66, 184, 232)
                        : Color.FromRgb(66, 184, 232);
                var model = new GeometryModel3D(
                    CreateSphereMesh(_posePoints[alias], selected ? 0.105 : 0.07),
                    CreateMaterial(jointColor));
                PoseWorld.Children.Add(model);
                if (!string.IsNullOrEmpty(actualNames[alias]))
                    _poseModelBones[model] = actualNames[alias];
            }
        }

        if (!string.IsNullOrWhiteSpace(_selectedBone))
        {
            var selectedAlias = actualNames.FirstOrDefault(pair => pair.Value == _selectedBone).Key;
            if (!string.IsNullOrEmpty(selectedAlias) && _posePoints.TryGetValue(selectedAlias, out var center))
            {
                var rotation = rotations[selectedAlias];
                AddGizmoAxis(center, center + TransformVector(new Vector3D(0.42, 0.0, 0.0), rotation), Colors.IndianRed);
                AddGizmoAxis(center, center + TransformVector(new Vector3D(0.0, 0.42, 0.0), rotation), Colors.LightGreen);
                AddGizmoAxis(center, center + TransformVector(new Vector3D(0.0, 0.0, 0.42), rotation), Colors.CornflowerBlue);
            }
        }
    }

    private bool IsImportedMotion()
    {
        var name = Path.GetFileNameWithoutExtension(_document.FilePath);
        return name.Contains("sword_shield_", StringComparison.OrdinalIgnoreCase) &&
            !name.Contains("_safe", StringComparison.OrdinalIgnoreCase) &&
            !name.Contains("idle", StringComparison.OrdinalIgnoreCase);
    }

    private void AddGizmoAxis(Point3D start, Point3D end, Color color)
    {
        PoseWorld.Children.Add(new GeometryModel3D(
            CreatePrismMesh(start, end, 0.012, 0.012),
            CreateMaterial(color)));
    }

    private void UpdateCamera()
    {
        if (PoseCamera is null)
            return;
        var cosPitch = Math.Cos(_cameraPitch);
        var offset = _modelUsesZUpCoordinates
            ? new Vector3D(
                Math.Sin(_cameraYaw) * cosPitch * _cameraDistance,
                Math.Sin(_cameraPitch) * _cameraDistance,
                Math.Cos(_cameraYaw) * cosPitch * _cameraDistance)
            : new Vector3D(
                Math.Sin(_cameraYaw) * cosPitch * _cameraDistance,
                Math.Sin(_cameraPitch) * _cameraDistance,
                Math.Cos(_cameraYaw) * cosPitch * _cameraDistance);
        PoseCamera.Position = new Point3D(
            _cameraTarget.X + offset.X,
            _cameraTarget.Y + offset.Y,
            _cameraTarget.Z + offset.Z);
        PoseCamera.LookDirection = new Vector3D(-offset.X, -offset.Y, -offset.Z);
        PoseCamera.UpDirection = _modelUsesZUpCoordinates
            ? new Vector3D(0.0, 1.0, 0.0)
            : new Vector3D(0.0, 1.0, 0.0);
    }

    private string FindHitBone(Point point)
    {
        var hit = VisualTreeHelper.HitTest(PoseViewport, point) as RayHitTestResult;
        return hit?.ModelHit is Model3D model && _poseModelBones.TryGetValue(model, out var bone)
            ? bone
            : string.Empty;
    }

    private void SelectBoneFromViewport(string boneName)
    {
        if (string.Equals(_selectedBone, boneName, StringComparison.Ordinal))
            return;
        _selectedBone = boneName;
        _suppressBoneSelection = true;
        BonesList.SelectedItem = boneName;
        _suppressBoneSelection = false;
        RefreshView();
    }

    private static NumericsQuaternion ToQuaternion(Vector3 rotation) =>
        NumericsQuaternion.Normalize(NumericsQuaternion.CreateFromYawPitchRoll(rotation.Y, rotation.X, rotation.Z));

    private static Vector3 SanitizeScale(Vector3 scale) => new(
        Math.Clamp(float.IsFinite(scale.X) ? scale.X : 1.0f, 0.05f, 3.0f),
        Math.Clamp(float.IsFinite(scale.Y) ? scale.Y : 1.0f, 0.05f, 3.0f),
        Math.Clamp(float.IsFinite(scale.Z) ? scale.Z : 1.0f, 0.05f, 3.0f));

    private static Point3D ToPoint3D(Vector3 point) => new(point.X, point.Y, point.Z);

    private static Vector3D TransformVector(Vector3D vector, NumericsQuaternion rotation)
    {
        var transformed = System.Numerics.Vector3.Transform(
            new Vector3((float)vector.X, (float)vector.Y, (float)vector.Z), rotation);
        return new Vector3D(transformed.X, transformed.Y, transformed.Z);
    }

    private static Material CreateMaterial(Color color)
    {
        var group = new MaterialGroup();
        group.Children.Add(new DiffuseMaterial(new SolidColorBrush(color)));
        group.Children.Add(new EmissiveMaterial(new SolidColorBrush(Color.FromArgb(42, color.R, color.G, color.B))));
        return group;
    }

    private static MeshGeometry3D CreatePrismMesh(Point3D start, Point3D end, double width, double depth)
    {
        var direction = end - start;
        if (direction.Length < 0.0001)
            direction = new Vector3D(0.0, 0.0001, 0.0);
        direction.Normalize();
        var side = Vector3D.CrossProduct(direction, new Vector3D(0.0, 1.0, 0.0));
        if (side.Length < 0.0001)
            side = Vector3D.CrossProduct(direction, new Vector3D(1.0, 0.0, 0.0));
        side.Normalize();
        side *= width * 0.5;
        var other = Vector3D.CrossProduct(direction, side);
        other.Normalize();
        other *= depth * 0.5;

        var mesh = new MeshGeometry3D();
        var startSide = new[] { start - side - other, start + side - other, start + side + other, start - side + other };
        var endSide = new[] { end - side - other, end + side - other, end + side + other, end - side + other };
        AddQuad(mesh, startSide[0], startSide[1], startSide[2], startSide[3], -direction);
        AddQuad(mesh, endSide[0], endSide[3], endSide[2], endSide[1], direction);
        AddQuad(mesh, startSide[0], endSide[0], endSide[1], startSide[1], -other);
        AddQuad(mesh, startSide[1], endSide[1], endSide[2], startSide[2], side);
        AddQuad(mesh, startSide[2], endSide[2], endSide[3], startSide[3], other);
        AddQuad(mesh, startSide[3], endSide[3], endSide[0], startSide[0], -side);
        return mesh;
    }

    private static void AddQuad(MeshGeometry3D mesh, Point3D a, Point3D b, Point3D c, Point3D d, Vector3D normal)
    {
        var start = mesh.Positions.Count;
        mesh.Positions.Add(a);
        mesh.Positions.Add(b);
        mesh.Positions.Add(c);
        mesh.Positions.Add(d);
        mesh.Normals.Add(normal);
        mesh.Normals.Add(normal);
        mesh.Normals.Add(normal);
        mesh.Normals.Add(normal);
        mesh.TriangleIndices.Add(start);
        mesh.TriangleIndices.Add(start + 1);
        mesh.TriangleIndices.Add(start + 2);
        mesh.TriangleIndices.Add(start);
        mesh.TriangleIndices.Add(start + 2);
        mesh.TriangleIndices.Add(start + 3);
    }

    private static MeshGeometry3D CreateSphereMesh(Point3D center, double radius)
    {
        const int latitudeSegments = 8;
        const int longitudeSegments = 12;
        var mesh = new MeshGeometry3D();
        for (var latitude = 0; latitude <= latitudeSegments; latitude++)
        {
            var theta = Math.PI * latitude / latitudeSegments;
            var sinTheta = Math.Sin(theta);
            var cosTheta = Math.Cos(theta);
            for (var longitude = 0; longitude <= longitudeSegments; longitude++)
            {
                var phi = 2.0 * Math.PI * longitude / longitudeSegments;
                var normal = new Vector3D(sinTheta * Math.Cos(phi), cosTheta, sinTheta * Math.Sin(phi));
                mesh.Positions.Add(center + normal * radius);
                mesh.Normals.Add(normal);
            }
        }
        for (var latitude = 0; latitude < latitudeSegments; latitude++)
        {
            for (var longitude = 0; longitude < longitudeSegments; longitude++)
            {
                var first = latitude * (longitudeSegments + 1) + longitude;
                var second = first + longitudeSegments + 1;
                mesh.TriangleIndices.Add(first);
                mesh.TriangleIndices.Add(second);
                mesh.TriangleIndices.Add(first + 1);
                mesh.TriangleIndices.Add(first + 1);
                mesh.TriangleIndices.Add(second);
                mesh.TriangleIndices.Add(second + 1);
            }
        }
        return mesh;
    }

    private static MeshGeometry3D CreateFloorMesh()
    {
        var mesh = new MeshGeometry3D();
        mesh.Positions.Add(new Point3D(-2.0, -0.82, -2.0));
        mesh.Positions.Add(new Point3D(2.0, -0.82, -2.0));
        mesh.Positions.Add(new Point3D(2.0, -0.82, 2.0));
        mesh.Positions.Add(new Point3D(-2.0, -0.82, 2.0));
        for (var i = 0; i < 4; i++)
            mesh.Normals.Add(new Vector3D(0.0, 1.0, 0.0));
        mesh.TriangleIndices.Add(0);
        mesh.TriangleIndices.Add(1);
        mesh.TriangleIndices.Add(2);
        mesh.TriangleIndices.Add(0);
        mesh.TriangleIndices.Add(2);
        mesh.TriangleIndices.Add(3);
        return mesh;
    }

    private string FindBoneForAlias(string alias)
    {
        var normalizedAlias = alias.Replace("1", "01", StringComparison.Ordinal)
            .Replace("2", "02", StringComparison.Ordinal);
        return _document.BoneNames.FirstOrDefault(name =>
        {
            var normalized = NormalizeBone(name);
            return normalized.Contains(normalizedAlias, StringComparison.Ordinal);
        }) ?? string.Empty;
    }

    private static string NormalizeBone(string name) =>
        name.ToLowerInvariant().Replace("mixamorig:", string.Empty).Replace("_", string.Empty).Replace(".", string.Empty);

    private static void AddLine(Canvas canvas, double x1, double y1, double x2, double y2, Color color, double thickness)
    {
        canvas.Children.Add(new Line
        {
            X1 = x1, Y1 = y1, X2 = x2, Y2 = y2,
            Stroke = new SolidColorBrush(color), StrokeThickness = thickness
        });
    }

    private static void AddText(Canvas canvas, string text, double x, double y, double size, Brush color)
    {
        var label = new TextBlock { Text = text, FontSize = size, Foreground = color };
        Canvas.SetLeft(label, x);
        Canvas.SetTop(label, y);
        canvas.Children.Add(label);
    }

    private void SetStatus(string message) => StatusText.Text = message;

    private static string FindProjectRoot()
    {
        var candidates = new[] { Environment.CurrentDirectory, AppContext.BaseDirectory };
        foreach (var candidate in candidates)
        {
            var directory = new DirectoryInfo(candidate);
            while (directory is not null)
            {
                if (Directory.Exists(Path.Combine(directory.FullName, "assets", "motion")))
                    return directory.FullName;
                directory = directory.Parent;
            }
        }
        return Environment.CurrentDirectory;
    }
}
