unit OBCListRecords;

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

function Initialize: integer;
var
  f, rec: IInterface;
  targetName, outPath: string;
  i: integer;
  sl: TStringList;
begin
  Result := 0;
  try
    targetName := GetArg('target-plugin');
    outPath := GetArg('out');
    if (targetName = '') or (outPath = '') then begin
      AddMessage('need --target-plugin and --out');
      Result := 1;
      Exit;
    end;
    f := FindFileByName(targetName);
    if not Assigned(f) then begin
      AddMessage('target not found: ' + targetName);
      Result := 2;
      Exit;
    end;
    sl := TStringList.Create;
    try
      sl.Add('plugin=' + targetName);
      sl.Add('record_count=' + IntToStr(RecordCount(f)));
      for i := 0 to Pred(RecordCount(f)) do begin
        rec := RecordByIndex(f, i);
        sl.Add(Signature(rec) + #9 + IntToHex(FormID(rec), 8) + #9 + EditorID(rec));
      end;
      sl.SaveToFile(outPath);
    finally
      sl.Free;
    end;
  except
    on e: Exception do begin
      AddMessage('FATAL: ' + e.Message);
      Result := 99;
    end;
  end;
end;

function Process(e: IInterface): integer; begin Result := 0; end;
function Finalize: integer; begin Result := 0; end;

end.
