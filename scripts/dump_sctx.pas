{
  dump_sctx.pas

  Reads a SCPT record by FormID and writes its SCTX subrecord to disk
  as plain UTF-8 text. Used to capture the canonical source of a script
  override so it can be committed to source control and re-applied
  losslessly via `author_full_sctx.pas`.

  Args:
    --master-formid=<hex>     required (LO FormID of the master record,
                              e.g. 1A012EAF for PSMainQuestDelayer.esp)
    --target-plugin=<name>    optional. If set, dumps the override of the
                              master that lives in this plugin. If unset,
                              dumps the master record itself.
    --out=<path>              required (where to write the .txt)

  Run via xEdit headless:
    TES4Edit_patched -IKnowWhatImDoing -autoload -autoexit \
      -D:<data> -P:<plugins.txt> \
      -script:dump_sctx.pas \
      --formid=01012EAF --out="D:\\path\\to\\Script.txt"

  IMPORTANT: do not place curly braces inside this block comment.
}
unit OBCDumpSctx;

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

function ParseFormID(const s: string): cardinal;
var body: string;
begin
  body := s;
  if (Length(body) >= 2) and ((body[2] = 'x') or (body[2] = 'X')) then
    body := Copy(body, 3, Length(body));
  Result := StrToInt('$' + body);
end;

function LookupByLoFormID(fid: cardinal): IInterface;
var
  loIdx: integer;
  pluginFile: IInterface;
begin
  Result := nil;
  loIdx := (fid shr 24) and $FF;
  if loIdx >= FileCount then Exit;
  pluginFile := FileByLoadOrder(loIdx);
  if not Assigned(pluginFile) then Exit;
  Result := RecordByFormID(pluginFile, fid, False);
  if not Assigned(Result) then
    Result := RecordByFormID(pluginFile, fid, True);
end;

function FindOverrideInPlugin(MasterRec: IInterface; const TargetName: string): IInterface;
var
  i, n: integer;
  ovr: IInterface;
begin
  Result := nil;
  n := OverrideCount(MasterRec);
  for i := 0 to n - 1 do begin
    ovr := OverrideByIndex(MasterRec, i);
    if Assigned(ovr) and SameText(GetFileName(GetFile(ovr)), TargetName) then begin
      Result := ovr;
      Exit;
    end;
  end;
end;

function Initialize: integer;
var
  Rec, SCTXElem: IInterface;
  FormIDStr, TargetName, OutPath, Sctx: string;
  Fid: cardinal;
  sl: TStringList;
begin
  FormIDStr := GetArg('master-formid');
  TargetName := GetArg('target-plugin');
  OutPath := GetArg('out');
  if (FormIDStr = '') or (OutPath = '') then begin
    AddMessage('ERROR: required: --master-formid=HEX --out=PATH (--target-plugin=NAME optional)');
    Result := 1;
    Exit;
  end;

  Fid := ParseFormID(FormIDStr);
  Rec := LookupByLoFormID(Fid);
  if not Assigned(Rec) then begin
    AddMessage('ERROR: master record not found for FormID ' + FormIDStr);
    Result := 2;
    Exit;
  end;

  if TargetName <> '' then begin
    Rec := FindOverrideInPlugin(Rec, TargetName);
    if not Assigned(Rec) then begin
      AddMessage('ERROR: no override of ' + FormIDStr + ' found in ' + TargetName);
      Result := 3;
      Exit;
    end;
    AddMessage('Resolved override in ' + TargetName);
  end;

  SCTXElem := ElementByPath(Rec, 'SCTX');
  if not Assigned(SCTXElem) then begin
    AddMessage('ERROR: record has no SCTX element');
    Result := 3;
    Exit;
  end;

  Sctx := GetEditValue(SCTXElem);
  AddMessage('Source: ' + GetFileName(GetFile(Rec)) + ' / ' + EditorID(Rec));
  AddMessage('SCTX length: ' + IntToStr(Length(Sctx)));

  sl := TStringList.Create;
  try
    sl.Text := Sctx;
    sl.SaveToFile(OutPath);
  finally
    sl.Free;
  end;

  AddMessage('Wrote ' + OutPath);
  Result := 0;
end;

function Process(e: IInterface): integer;
begin
  Result := 0;
end;

end.
