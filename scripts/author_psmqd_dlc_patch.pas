{
  author_psmqd_dlc_patch.pas

  Authors the 7-record APW-pattern PSMQD <-> Delay DLC Start patch
  into a target plugin in a single xEdit invocation.

  Records authored:
    PSCharactergenStartingEquipmentScript  (PSMainQuestDelayer.esp)
    DLCHorseArmorScript                    (DLCHorseArmor.esp)
    DLCOrreryQuestScript                   (DLCOrrery.esp)
    DLCDeepscornScript                     (DLCVileLair.esp)
    DLC06ThievesDenQuestScript             (DLCThievesDen.esp)
    DLCBattlehornCastleScript              (DLCBattlehornCastle.esp)
    DLCFrostcragSpireScript                (DLCFrostcrag.esp)

  Args:
    --target-plugin=<filename>   required
    --sources-dir=<path>         required. Directory holding
                                 <EDID>.txt SCTX source files.
    --remove-edid=<EDID>         optional, repeatable. Existing
                                 SCPT overrides in the target with
                                 these EDIDs are removed (used to
                                 retire obsolete records).

  IMPORTANT: do not place curly braces inside this block comment.
}
unit OBCAuthorPSMQDPatch;

const
  TOOL_VERSION = '0.1.0';

  REC_COUNT = 8;

function GetArg(const key: string): string;
var
  i: integer;
  prefix, s: string;
begin
  Result := '';
  prefix := '--' + key + '=';
  for i := 0 to ParamCount do begin
    s := ParamStr(i);
    if Copy(s, 1, Length(prefix)) = prefix then begin
      Result := Copy(s, Length(prefix) + 1, Length(s));
      Exit;
    end;
  end;
end;

function FindFileByName(const name: string): IInterface;
var
  i: integer;
begin
  Result := nil;
  for i := 0 to Pred(FileCount) do
    if SameText(GetFileName(FileByIndex(i)), name) then begin
      Result := FileByIndex(i);
      Exit;
    end;
end;

function FindRecordByEdidInFile(const f: IInterface; const edid: string): IInterface;
var
  i: integer;
  rec: IInterface;
begin
  Result := nil;
  for i := 0 to Pred(RecordCount(f)) do begin
    rec := RecordByIndex(f, i);
    if SameText(EditorID(rec), edid) then begin
      Result := rec;
      Exit;
    end;
  end;
end;

function LoadSctxFile(const path: string): string;
var
  sl: TStringList;
begin
  Result := '';
  if not FileExists(path) then begin
    AddMessage('ERROR: SCTX file missing: ' + path);
    Exit;
  end;
  sl := TStringList.Create;
  try
    sl.LoadFromFile(path);
    Result := sl.Text;
  finally
    sl.Free;
  end;
end;

function AuthorOne(const target: IInterface;
                   const masterPlugin, edid, sctxPath: string): boolean;
var
  masterFile, masterRec, override, sctxElem: IInterface;
  newSctx: string;
begin
  Result := False;

  masterFile := FindFileByName(masterPlugin);
  if not Assigned(masterFile) then begin
    AddMessage('ERROR: master plugin not loaded: ' + masterPlugin);
    Exit;
  end;

  masterRec := FindRecordByEdidInFile(masterFile, edid);
  if not Assigned(masterRec) then begin
    AddMessage('ERROR: record not found in ' + masterPlugin + ': ' + edid);
    Exit;
  end;

  AddMasterIfMissing(target, masterPlugin);

  override := wbCopyElementToFile(masterRec, target, False, True);
  if not Assigned(override) then begin
    AddMessage('ERROR: wbCopyElementToFile returned nil for ' + edid);
    Exit;
  end;

  sctxElem := ElementByPath(override, 'SCTX');
  if not Assigned(sctxElem) then begin
    AddMessage('ERROR: override has no SCTX element: ' + edid);
    Exit;
  end;

  newSctx := LoadSctxFile(sctxPath);
  if newSctx = '' then begin
    AddMessage('ERROR: empty SCTX from: ' + sctxPath);
    Exit;
  end;

  SetEditValue(sctxElem, newSctx);
  AddMessage('  + ' + edid + ' (' + IntToStr(Length(newSctx)) + ' chars from ' + ExtractFileName(sctxPath) + ')');
  Result := True;
end;

procedure RemoveByEdids(const target: IInterface);
var
  i, j: integer;
  rec: IInterface;
  arg, prefix, edid: string;
  removed: integer;
begin
  removed := 0;
  prefix := '--remove-edid=';
  for i := 0 to ParamCount do begin
    arg := ParamStr(i);
    if Copy(arg, 1, Length(prefix)) <> prefix then Continue;
    edid := Copy(arg, Length(prefix) + 1, Length(arg));

    for j := Pred(RecordCount(target)) downto 0 do begin
      rec := RecordByIndex(target, j);
      if SameText(EditorID(rec), edid) then begin
        AddMessage('  - removing existing override: ' + edid);
        Remove(rec);
        Inc(removed);
        Break;
      end;
    end;
  end;
  if removed > 0 then
    AddMessage('Removed ' + IntToStr(removed) + ' obsolete override(s).');
end;

function AuthorByIndex(const target: IInterface; const sourcesDir: string; idx: integer): boolean;
var
  edid, masterPlugin: string;
begin
  Result := False;
  case idx of
    0: begin edid := 'PSCharactergenStartingEquipmentScript'; masterPlugin := 'PSMainQuestDelayer.esp'; end;
    1: begin edid := 'DLCHorseArmorScript';                    masterPlugin := 'DLCHorseArmor.esp'; end;
    2: begin edid := 'DLCOrreryQuestScript';                   masterPlugin := 'DLCOrrery.esp'; end;
    3: begin edid := 'DLCDeepscornScript';                     masterPlugin := 'DLCVileLair.esp'; end;
    4: begin edid := 'DLC06ThievesDenQuestScript';             masterPlugin := 'DLCThievesDen.esp'; end;
    5: begin edid := 'DLCBattlehornCastleScript';              masterPlugin := 'DLCBattlehornCastle.esp'; end;
    6: begin edid := 'DLCFrostcragSpireScript';                masterPlugin := 'DLCFrostcrag.esp'; end;
    7: begin edid := 'DL9QuestSCRIPT';                         masterPlugin := 'DLCMehrunesRazor.esp'; end;
  else
    Exit;
  end;
  Result := AuthorOne(target, masterPlugin, edid, sourcesDir + edid + '.txt');
end;

function Initialize: integer;
var
  target: IInterface;
  targetName, sourcesDir: string;
  ok, fail, i: integer;
begin
  Result := 0;
  try
    AddMessage('OBCAuthorPSMQDPatch v' + TOOL_VERSION);

    targetName := GetArg('target-plugin');
    sourcesDir := GetArg('sources-dir');
    if (targetName = '') or (sourcesDir = '') then begin
      AddMessage('ERROR: need --target-plugin=NAME --sources-dir=PATH');
      Result := 1;
      Exit;
    end;
    // JvI doesn't support s[i] char indexing; use Copy.
    if Length(sourcesDir) > 0 then begin
      if (Copy(sourcesDir, Length(sourcesDir), 1) <> '\') and
         (Copy(sourcesDir, Length(sourcesDir), 1) <> '/') then
        sourcesDir := sourcesDir + '\';
    end;

    AddMessage('Target: ' + targetName);
    AddMessage('Sources: ' + sourcesDir);

    target := FindFileByName(targetName);
    if not Assigned(target) then begin
      AddMessage('Target plugin not in load order; creating: ' + targetName);
      target := AddNewFileName(targetName);
      if not Assigned(target) then begin
        AddMessage('ERROR: AddNewFileName returned nil');
        Result := 2;
        Exit;
      end;
    end;

    RemoveByEdids(target);

    ok := 0;
    fail := 0;
    for i := 0 to REC_COUNT - 1 do begin
      if AuthorByIndex(target, sourcesDir, i) then
        Inc(ok)
      else
        Inc(fail);
    end;

    AddMessage('Authored ' + IntToStr(ok) + ' / ' + IntToStr(REC_COUNT) +
               ' records (' + IntToStr(fail) + ' failed).');
    if fail > 0 then Result := 3;
  except
    on e: Exception do begin
      AddMessage('FATAL: ' + e.Message);
      Result := 99;
    end;
  end;
end;

function Process(e: IInterface): integer;
begin
  Result := 0;
end;

function Finalize: integer;
begin
  Result := 0;
end;

end.
